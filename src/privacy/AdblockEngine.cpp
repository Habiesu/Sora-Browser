#include "privacy/AdblockEngine.h"

#include <sora_adblock.h>

namespace sora {

void AdblockEngine::Deleter::operator()(SoraAdblock* p) const { sora_adblock_free(p); }

AdblockEngine AdblockEngine::fromRules(const QByteArray& rules)
{
    return AdblockEngine(sora_adblock_from_rules(rules.constData(), size_t(rules.size())));
}

std::optional<AdblockEngine> AdblockEngine::deserialize(const QByteArray& data)
{
    SoraAdblock* raw = sora_adblock_deserialize(reinterpret_cast<const uint8_t*>(data.constData()),
                                                size_t(data.size()));
    if (!raw)
        return std::nullopt;
    return AdblockEngine(raw);
}

QByteArray AdblockEngine::serialize() const
{
    uint8_t* buf = nullptr;
    const size_t len = sora_adblock_serialize(m_engine.get(), &buf);
    QByteArray out(reinterpret_cast<const char*>(buf), qsizetype(len));
    sora_adblock_free_buffer(buf, len);
    return out;
}

bool AdblockEngine::shouldBlock(const QUrl& url, const QUrl& firstParty, const char* requestType) const
{
    const QByteArray u = url.toEncoded();
    const QByteArray src = firstParty.toEncoded();
    return sora_adblock_should_block(m_engine.get(), u.constData(), src.constData(), requestType);
}

} // namespace sora
