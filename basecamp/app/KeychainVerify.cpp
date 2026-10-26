#include "KeychainVerify.h"

#include <openssl/evp.h>

#include <memory>

namespace KeychainVerify {

bool verify(const QString& algorithm, const QByteArray& publicKey,
            const QByteArray& message, const QByteArray& signature)
{
    if (algorithm != QStringLiteral("ed25519")) return false;
    if (publicKey.size() != 32 || signature.size() != 64) return false;

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                    reinterpret_cast<const unsigned char*>(publicKey.constData()),
                                    static_cast<size_t>(publicKey.size())),
        &EVP_PKEY_free);
    if (!key) return false;

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx) return false;
    // Ed25519 is one-shot: no digest is named, and the whole message goes to
    // EVP_DigestVerify at once.
    if (EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) != 1) return false;
    return EVP_DigestVerify(ctx.get(),
                            reinterpret_cast<const unsigned char*>(signature.constData()),
                            static_cast<size_t>(signature.size()),
                            reinterpret_cast<const unsigned char*>(message.constData()),
                            static_cast<size_t>(message.size())) == 1;
}

} // namespace KeychainVerify
