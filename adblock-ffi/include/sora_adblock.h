/* API C del motor de bloqueo de Sora (envoltorio de adblock-rust de Brave). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SoraAdblock SoraAdblock;

/* Compila un motor a partir de reglas en sintaxis Adblock Plus. Nunca devuelve NULL. */
SoraAdblock* sora_adblock_from_rules(const char* rules, size_t len);
/* Reconstruye un motor serializado. Devuelve NULL si los datos no son válidos. */
SoraAdblock* sora_adblock_deserialize(const uint8_t* data, size_t len);
/* Serializa el motor; el búfer se libera con sora_adblock_free_buffer. Devuelve su tamaño. */
size_t sora_adblock_serialize(const SoraAdblock* engine, uint8_t** out);
void sora_adblock_free_buffer(uint8_t* buf, size_t len);
/* request_type: document subdocument stylesheet script image font object
   xmlhttprequest ping media websocket other. Seguro entre hilos. */
bool sora_adblock_should_block(const SoraAdblock* engine, const char* url,
                               const char* source_url, const char* request_type);
void sora_adblock_free(SoraAdblock* engine);

#ifdef __cplusplus
}
#endif
