#pragma once

#include <QRect>
#include <QSize>

#include <algorithm>

// The desktop layout's Basecamp overlay (basecamp-experiments#43): where its
// panel sits in a window of `area`. Centred, 70% of the width kept between 840
// and 1000 px (never closer than 24 px to an edge), and the height between the
// top bar (`top`) and the dock (`bottom`), 24 px clear of each. The home app's
// view is placed `kInset` px inside the panel, so the panel's rounded corners
// stay visible.
//
// The Activity sidebar (basecamp-experiments#44) is the same home app in its
// feed mode: a phone-wide column on the window's right edge, from under the
// top bar to the bottom, with a 1 px border on its left.
namespace HomeOverlayGeometry {

constexpr int kMinWidth = 840;
constexpr int kMaxWidth = 1000;
constexpr int kEdge = 24;
constexpr int kInset = 8;
constexpr int kSidebarWidth = 400;

inline QRect panel(const QSize& area, int top = 0, int bottom = 0)
{
    const int room = std::max(0, area.width() - 2 * kEdge);
    const int wanted = std::clamp(area.width() * 7 / 10, kMinWidth, kMaxWidth);
    const int w = std::min(wanted, room);
    const int h = std::max(0, area.height() - top - bottom - 2 * kEdge);
    return QRect((area.width() - w) / 2, top + kEdge, w, h);
}

inline QRect content(const QSize& area, int top = 0, int bottom = 0)
{
    return panel(area, top, bottom).adjusted(kInset, kInset, -kInset, -kInset);
}

inline QRect sidebar(const QSize& area, int top = 0)
{
    const int w = std::clamp(kSidebarWidth, 0, std::max(0, area.width()));
    const int h = std::max(0, area.height() - top);
    return QRect(area.width() - w, top, w, h);
}

inline QRect sidebarContent(const QSize& area, int top = 0)
{
    const QRect s = sidebar(area, top);
    return s.width() > 1 ? s.adjusted(1, 0, 0, 0) : s;
}

} // namespace HomeOverlayGeometry
