//! API C del motor de bloqueo de Sora. Ver `include/sora_adblock.h`.
//!
//! Ninguna función deja escapar un pánico hacia C: ante cualquier error se devuelve
//! "no bloquear" o NULL, porque un fallo del bloqueador nunca debe tumbar el navegador.

use std::ffi::{c_char, CStr};
use std::panic::{catch_unwind, AssertUnwindSafe};

use adblock::lists::{FilterSet, ParseOptions};
use adblock::request::Request;
use adblock::Engine;

/// Motor opaco para C. `Engine` es `Send + Sync` (feature "single-thread" desactivada).
pub struct SoraAdblock {
    engine: Engine,
}

fn boxed(engine: Engine) -> *mut SoraAdblock {
    Box::into_raw(Box::new(SoraAdblock { engine }))
}

#[no_mangle]
pub extern "C" fn sora_adblock_from_rules(rules: *const c_char, len: usize) -> *mut SoraAdblock {
    let result = catch_unwind(|| {
        let text: String = if rules.is_null() || len == 0 {
            String::new()
        } else {
            let bytes = unsafe { std::slice::from_raw_parts(rules as *const u8, len) };
            String::from_utf8_lossy(bytes).into_owned()
        };
        let mut set = FilterSet::new(false);
        set.add_filter_list(text, ParseOptions::default());
        Engine::new_with_filter_set(set)
    });
    boxed(result.unwrap_or_else(|_| Engine::default()))
}

#[no_mangle]
pub extern "C" fn sora_adblock_deserialize(data: *const u8, len: usize) -> *mut SoraAdblock {
    if data.is_null() || len == 0 {
        return std::ptr::null_mut();
    }
    let result = catch_unwind(|| {
        let bytes = unsafe { std::slice::from_raw_parts(data, len) };
        let mut engine = Engine::default();
        engine.deserialize(bytes).ok().map(|_| engine)
    });
    match result {
        Ok(Some(engine)) => boxed(engine),
        _ => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn sora_adblock_serialize(engine: *const SoraAdblock, out: *mut *mut u8) -> usize {
    if engine.is_null() || out.is_null() {
        return 0;
    }
    let result = catch_unwind(AssertUnwindSafe(|| unsafe { &*engine }.engine.serialize()));
    match result {
        Ok(bytes) => {
            let boxed: Box<[u8]> = bytes.into_boxed_slice();
            let len = boxed.len();
            unsafe { *out = Box::into_raw(boxed) as *mut u8 };
            len
        }
        Err(_) => {
            unsafe { *out = std::ptr::null_mut() };
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn sora_adblock_free_buffer(buf: *mut u8, len: usize) {
    if !buf.is_null() {
        unsafe { drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(buf, len))) };
    }
}

fn is_blockable_scheme(url: &str) -> bool {
    ["http://", "https://", "ws://", "wss://"]
        .iter()
        .any(|p| url.len() >= p.len() && url[..p.len()].eq_ignore_ascii_case(p))
}

#[no_mangle]
pub extern "C" fn sora_adblock_should_block(
    engine: *const SoraAdblock,
    url: *const c_char,
    source_url: *const c_char,
    request_type: *const c_char,
) -> bool {
    if engine.is_null() || url.is_null() {
        return false;
    }
    catch_unwind(AssertUnwindSafe(|| {
        let to_str = |p: *const c_char| {
            if p.is_null() { "" } else { unsafe { CStr::from_ptr(p) }.to_str().unwrap_or("") }
        };
        let url = to_str(url);
        if !is_blockable_scheme(url) {
            return false;
        }
        let ty = match to_str(request_type) {
            "" => "other",
            t => t,
        };
        match Request::new(url, to_str(source_url), ty, "GET") {
            Ok(request) => unsafe { &*engine }.engine.check_network_request(&request).should_block(),
            Err(_) => false,
        }
    }))
    .unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn sora_adblock_free(engine: *mut SoraAdblock) {
    if !engine.is_null() {
        unsafe { drop(Box::from_raw(engine)) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;
    use std::ptr;

    const RULES: &str = "||ads.example^\n@@||ads.example/ok^\n";

    fn engine() -> *mut SoraAdblock {
        sora_adblock_from_rules(RULES.as_ptr() as *const _, RULES.len())
    }

    fn check(e: *const SoraAdblock, url: &str, source: &str, ty: &str) -> bool {
        let (u, s, t) = (CString::new(url).unwrap(), CString::new(source).unwrap(), CString::new(ty).unwrap());
        sora_adblock_should_block(e, u.as_ptr(), s.as_ptr(), t.as_ptr())
    }

    #[test]
    fn blocks_matching() {
        let e = engine();
        assert!(check(e, "https://ads.example/x.js", "https://site.test", "script"));
        sora_adblock_free(e);
    }

    #[test]
    fn exception_wins() {
        let e = engine();
        assert!(!check(e, "https://ads.example/ok/x.js", "https://site.test", "script"));
        sora_adblock_free(e);
    }

    #[test]
    fn non_http_never_blocks() {
        let e = engine();
        for url in ["data:text/plain,hi", "blob:https://site.test/1", "about:blank", "file:///etc/hosts", "chrome-extension://abc/x.js"] {
            assert!(!check(e, url, "https://site.test", "script"), "{url}");
        }
        sora_adblock_free(e);
    }

    #[test]
    fn null_arguments_never_block() {
        let e = engine();
        assert!(!sora_adblock_should_block(ptr::null(), ptr::null(), ptr::null(), ptr::null()));
        assert!(!sora_adblock_should_block(e, ptr::null(), ptr::null(), ptr::null()));
        sora_adblock_free(e);
    }

    #[test]
    fn serialize_roundtrip() {
        let e = engine();
        let mut buf: *mut u8 = ptr::null_mut();
        let len = sora_adblock_serialize(e, &mut buf);
        assert!(len > 0 && !buf.is_null());
        let e2 = sora_adblock_deserialize(buf, len);
        assert!(!e2.is_null());
        assert!(check(e2, "https://ads.example/x.js", "https://site.test", "script"));
        sora_adblock_free_buffer(buf, len);
        sora_adblock_free(e);
        sora_adblock_free(e2);
    }

    #[test]
    fn garbage_deserialize_is_null() {
        let junk = b"basura";
        assert!(sora_adblock_deserialize(junk.as_ptr(), junk.len()).is_null());
    }
}
