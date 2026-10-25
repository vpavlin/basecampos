#pragma once

#include <QMetaObject>
#include <QObject>
#include <QVariant>

// Esc on the desktop layout's Basecamp overlay and Activity sidebar
// (basecamp-experiments#47): with text in home's search, Esc clears it; with
// an empty field it closes the surface. The shell sees the key first (it owns
// Esc there), so it asks home before closing: home's root answers through
// escapeSearch() whether it had text to clear. A home app without that
// function, or one that answers anything but true, gets the old behaviour.
// Only a key home's view has (`inHome`) goes to home: with the focus in the
// dock or the top bar, Esc closes at once, as it did before.
namespace HomeEscape {

enum class Action { Ignore, Cleared, Close };

// Asks home to clear its search; true when it did.
inline bool clearInHome(QObject* homeRoot)
{
    if (!homeRoot)
        return false;
    QVariant cleared;
    if (!QMetaObject::invokeMethod(homeRoot, "escapeSearch", Q_RETURN_ARG(QVariant, cleared)))
        return false;
    return cleared.typeId() == QMetaType::Bool && cleared.toBool();
}

// What Esc does while home is `open`; a dialog up over it takes the key.
// `inHome`: the key went to home's view (it has the keyboard focus).
inline Action onEscape(bool open, bool dialogUp, bool inHome, QObject* homeRoot)
{
    if (!open || dialogUp)
        return Action::Ignore;
    if (!inHome)
        return Action::Close;
    return clearInHome(homeRoot) ? Action::Cleared : Action::Close;
}

} // namespace HomeEscape
