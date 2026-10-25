#pragma once

#include <QObject>
#include <QVariantMap>

class NotificationCenter;
class ObjectStore;

// Notifications in the home feed (basecamp-experiments #31): every notification
// an app posts is also a `notification` object, published on that app's behalf,
// so it shows as a card in the feed and in search. There is no bell or list.
//
// The two switches keep their meaning. Settings → Notifications (muted) stops
// a post before NotificationCenter emits it, and muting clears the app's cards
// here as it clears its list. Settings → Search (the store's switch) drops the
// cards like anything else the app publishes. Same tag, same object: replaced
// in place, and new again. Basecamp's own notices stay toasts only; a card for
// them would open an app that does not exist.
//
// QtCore only, like both sides, so notification_objects_test.cpp drives it.
class NotificationObjects : public QObject {
    Q_OBJECT
public:
    // Cards kept per app, newest first; NotificationCenter keeps as many.
    static constexpr int kMaxPerApp = 50;

    NotificationObjects(NotificationCenter* center, ObjectStore* store, QObject* parent = nullptr);

    // The object key for a notification: by tag when it has one (so a newer
    // one replaces it), else by its id. Prefixed so it cannot meet the app's
    // own keys by accident.
    static QString keyFor(const QVariantMap& notification);
    // The `notification` envelope for one NotificationCenter item.
    static QVariantMap envelopeFor(const QVariantMap& notification);
    // Whether `key` is one of ours.
    static bool isMirrorKey(const QString& key);

private:
    void mirror(const QVariantMap& notification);
    void trim(const QString& app);
    void dropMuted();

    NotificationCenter* m_center = nullptr;
    ObjectStore* m_store = nullptr;
};
