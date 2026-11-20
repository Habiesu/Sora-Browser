#pragma once
#include <QByteArray>
#include <QUrl>
#include <memory>
#include <optional>

struct SoraAdblock;

namespace sora {

// Envoltorio RAII del motor de bloqueo de Brave. shouldBlock() es seguro entre hilos.
class AdblockEngine {
public:
    static AdblockEngine fromRules(const QByteArray& rules);
    static std::optional<AdblockEngine> deserialize(const QByteArray& data);

    QByteArray serialize() const;
    bool shouldBlock(const QUrl& url, const QUrl& firstParty, const char* requestType) const;

private:
    struct Deleter { void operator()(SoraAdblock* p) const; };
    explicit AdblockEngine(SoraAdblock* raw) : m_engine(raw) {}
    std::unique_ptr<SoraAdblock, Deleter> m_engine;
};

} // namespace sora
