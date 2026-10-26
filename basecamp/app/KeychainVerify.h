#pragma once

#include <QByteArray>
#include <QString>

// Signature checks for the Keychain (basecamp-experiments#56): every signature
// a signer app hands back is checked here against the public key the app
// registered, before anything reaches the requester.
//
// OpenSSL EVP, which the app already links. Ed25519 only for now; any other
// algorithm answers false, so a key the shell cannot check never signs.
namespace KeychainVerify {

bool verify(const QString& algorithm, const QByteArray& publicKey,
            const QByteArray& message, const QByteArray& signature);

} // namespace KeychainVerify
