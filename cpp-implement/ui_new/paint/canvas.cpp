#include "paint/canvas.h"

#include "paint/text_draw.h"
#include "svg/path.h"
#include "svg/svg_paint.h"

#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <stack>

namespace ui {

namespace {

ULONG_PTR g_gdiplusToken = 0;
int g_graphicsUsers = 0;

Gdiplus::Color toGdiColor(Color color, float opacity) {
    float alpha = color.a * opacity;

    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    return Gdiplus::Color(
        static_cast<BYTE>(alpha),
        color.r, color.g, color.b
    );
}

void addRoundRect(
    Gdiplus::GraphicsPath& path,
    const RectF& r,
    const CornerRadii& radii
) {
    if (r.w <= 0 || r.h <= 0)
        return;

    float limit = std::min(r.w, r.h) / 2.0f;
    float tl = std::max(0.f, std::min(radii.tl, limit));
    float tr = std::max(0.f, std::min(radii.tr, limit));
    float br = std::max(0.f, std::min(radii.br, limit));
    float bl = std::max(0.f, std::min(radii.bl, limit));

    if (tl == 0 && tr == 0 && br == 0 && bl == 0) {
        path.AddRectangle(Gdiplus::RectF(r.x, r.y, r.w, r.h));
        return;
    }

    path.StartFigure();

    if (tl > 0) {
        path.AddArc(r.x, r.y, tl * 2, tl * 2, 180, 90);
    }
    else {
        path.AddLine(r.x, r.y, r.x, r.y);
    }

    path.AddLine(r.x + tl, r.y, r.right() - tr, r.y);

    if (tr > 0)
        path.AddArc(r.right() - tr * 2, r.y, tr * 2, tr * 2, 270, 90);
    else
        path.AddLine(r.right(), r.y, r.right(), r.y);

    path.AddLine(r.right(), r.y + tr, r.right(), r.bottom() - br);

    if (br > 0)
        path.AddArc(r.right() - br * 2, r.bottom() - br * 2, br * 2, br * 2, 0, 90);
    else
        path.AddLine(r.right(), r.bottom(), r.right(), r.bottom());

    path.AddLine(r.right() - br, r.bottom(), r.x + bl, r.bottom());

    if (bl > 0)
        path.AddArc(r.x, r.bottom() - bl * 2, bl * 2, bl * 2, 90, 90);
    else
        path.AddLine(r.x, r.bottom(), r.x, r.bottom());

    path.AddLine(r.x, r.bottom() - bl, r.x, r.y + tl);

    if (tl <= 0)
        path.AddLine(r.x, r.y, r.x, r.y);

    path.CloseFigure();
}

Gdiplus::LineCap toGdiCap(LineCap cap) {
    switch (cap) {
    case LineCap::Round: return Gdiplus::LineCapRound;
    case LineCap::Square: return Gdiplus::LineCapSquare;
    case LineCap::Butt:
    default: return Gdiplus::LineCapFlat;
    }
}

Gdiplus::LineJoin toGdiJoin(LineJoin join) {
    switch (join) {
    case LineJoin::Round: return Gdiplus::LineJoinRound;
    case LineJoin::Bevel: return Gdiplus::LineJoinBevel;
    case LineJoin::Miter:
    default: return Gdiplus::LineJoinMiter;
    }
}

} // namespace

struct Canvas::Impl {
    Gdiplus::Graphics* graphics = nullptr;
    HDC hdc = nullptr;
    std::stack<Gdiplus::GraphicsState> states;
    std::stack<int> gdiStates;
    std::stack<std::pair<float, float>> translations;
    float offsetX = 0, offsetY = 0;
    float opacity = 1.0f;
};

Canvas::Canvas(HDC hdc, int width, int height) {
    (void)width;
    (void)height;

    impl_ = new Impl();
    impl_->hdc = hdc;
    impl_->graphics = new Gdiplus::Graphics(hdc);
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    impl_->graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    impl_->graphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
}

Canvas::~Canvas() {
    if (impl_) {
        if (impl_->graphics) {
            impl_->graphics->Flush(Gdiplus::FlushIntentionSync);
            delete impl_->graphics;
        }

        delete impl_;
    }
}

void Canvas::fillRoundRect(const RectF& r, const CornerRadii& radii, Color c) {
    if (!impl_ || !impl_->graphics || r.w <= 0 || r.h <= 0 || c.a == 0)
        return;

    RectF shifted { r.x + impl_->offsetX, r.y + impl_->offsetY, r.w, r.h };
    Gdiplus::GraphicsPath path;
    addRoundRect(path, shifted, radii);
    Gdiplus::SolidBrush brush(toGdiColor(c, impl_->opacity));
    impl_->graphics->FillPath(&brush, &path);
}

void Canvas::strokeBorders(
    const RectF& r,
    const CornerRadii& radii,
    const Edges& widths,
    const Color colors[4]
) {
    if (!impl_ || !impl_->graphics)
        return;

    RectF sr { r.x + impl_->offsetX, r.y + impl_->offsetY, r.w, r.h };
    bool uniform = widths.top == widths.right &&
                   widths.right == widths.bottom &&
                   widths.bottom == widths.left &&
                   colors[0].r == colors[1].r && colors[1].r == colors[2].r &&
                   colors[2].r == colors[3].r &&
                   colors[0].g == colors[1].g && colors[1].g == colors[2].g &&
                   colors[2].g == colors[3].g &&
                   colors[0].b == colors[1].b && colors[1].b == colors[2].b &&
                   colors[2].b == colors[3].b &&
                   colors[0].a == colors[1].a && colors[1].a == colors[2].a &&
                   colors[2].a == colors[3].a;

    if (uniform) {
        float width = widths.top;

        if (width <= 0 || colors[0].a == 0)
            return;

        RectF inset {
            sr.x + width / 2,
            sr.y + width / 2,
            std::max(0.f, sr.w - width),
            std::max(0.f, sr.h - width)
        };

        CornerRadii inner = radii;
        inner.tl = std::max(0.f, inner.tl - width / 2);
        inner.tr = std::max(0.f, inner.tr - width / 2);
        inner.br = std::max(0.f, inner.br - width / 2);
        inner.bl = std::max(0.f, inner.bl - width / 2);

        Gdiplus::GraphicsPath path;
        addRoundRect(path, inset, inner);
        Gdiplus::Pen pen(toGdiColor(colors[0], impl_->opacity), width);
        pen.SetAlignment(Gdiplus::PenAlignmentCenter);
        impl_->graphics->DrawPath(&pen, &path);
        return;
    }

    auto line = [&](PointF a, PointF b, float w, Color c) {
        if (w <= 0 || c.a == 0)
            return;

        Gdiplus::Pen pen(toGdiColor(c, impl_->opacity), w);
        impl_->graphics->DrawLine(&pen, a.x, a.y, b.x, b.y);
    };

    float topY = sr.y + widths.top / 2;
    float rightX = sr.right() - widths.right / 2;
    float bottomY = sr.bottom() - widths.bottom / 2;
    float leftX = sr.x + widths.left / 2;

    line({ sr.x + radii.tl, topY }, { sr.right() - radii.tr, topY }, widths.top, colors[0]);
    line({ rightX, sr.y + radii.tr }, { rightX, sr.bottom() - radii.br }, widths.right, colors[1]);
    line({ sr.right() - radii.br, bottomY }, { sr.x + radii.bl, bottomY }, widths.bottom, colors[2]);
    line({ leftX, sr.bottom() - radii.bl }, { leftX, sr.y + radii.tl }, widths.left, colors[3]);
}

void Canvas::drawLine(PointF a, PointF b, float width, Color c) {
    if (!impl_ || !impl_->graphics || width <= 0 || c.a == 0)
        return;

    Gdiplus::Pen pen(toGdiColor(c, impl_->opacity), width);
    impl_->graphics->DrawLine(
        &pen,
        a.x + impl_->offsetX, a.y + impl_->offsetY,
        b.x + impl_->offsetX, b.y + impl_->offsetY
    );
}

void Canvas::fillEllipse(const RectF& r, Color c) {
    if (!impl_ || !impl_->graphics || c.a == 0)
        return;

    Gdiplus::SolidBrush brush(toGdiColor(c, impl_->opacity));
    impl_->graphics->FillEllipse(
        &brush,
        Gdiplus::RectF(
            r.x + impl_->offsetX,
            r.y + impl_->offsetY,
            r.w, r.h
        )
    );
}

void Canvas::pushClip(const RectF& r, const CornerRadii& radii) {
    if (!impl_ || !impl_->graphics)
        return;

    impl_->states.push(impl_->graphics->Save());

    RectF sr { r.x + impl_->offsetX, r.y + impl_->offsetY, r.w, r.h };
    Gdiplus::GraphicsPath path;
    addRoundRect(path, sr, radii);
    impl_->graphics->SetClip(&path, Gdiplus::CombineModeIntersect);

    if (impl_->hdc) {
        int saved = SaveDC(impl_->hdc);
        IntersectClipRect(
            impl_->hdc,
            static_cast<int>(std::floor(sr.x)),
            static_cast<int>(std::floor(sr.y)),
            static_cast<int>(std::ceil(sr.right())),
            static_cast<int>(std::ceil(sr.bottom()))
        );
        impl_->gdiStates.push(saved);
    }
}

void Canvas::popClip() {
    if (!impl_ || !impl_->graphics || impl_->states.empty())
        return;

    impl_->graphics->Restore(impl_->states.top());
    impl_->states.pop();

    if (impl_->hdc && !impl_->gdiStates.empty()) {
        RestoreDC(impl_->hdc, impl_->gdiStates.top());
        impl_->gdiStates.pop();
    }
}

void Canvas::drawPath(
    const Path& p,
    const StrokeStyle& stroke,
    Color fill,
    float scale,
    PointF translate
) {
    if (!impl_ || !impl_->graphics || p.segs.empty())
        return;

    Gdiplus::GraphicsPath path;
    Gdiplus::PointF last;

    for (const PathSeg& seg : p.segs) {
        auto tx = [&](const PointF& point) {
            return Gdiplus::PointF(
                translate.x + impl_->offsetX + point.x * scale,
                translate.y + impl_->offsetY + point.y * scale
            );
        };

        switch (seg.kind) {
        case SegKind::MoveTo: {
            last = tx(seg.p1);
            path.StartFigure();
            break;
        }
        case SegKind::LineTo: {
            Gdiplus::PointF point = tx(seg.p1);
            path.AddLine(last, point);
            last = point;
            break;
        }
        case SegKind::CubicTo: {
            Gdiplus::PointF c1 = tx(seg.p1);
            Gdiplus::PointF c2 = tx(seg.p2);
            Gdiplus::PointF end = tx(seg.p3);
            path.AddBezier(last, c1, c2, end);
            last = end;
            break;
        }
        case SegKind::Close:
            path.CloseFigure();
            break;
        }
    }

    if (fill.a > 0) {
        Gdiplus::SolidBrush brush(toGdiColor(fill, impl_->opacity));
        impl_->graphics->FillPath(&brush, &path);
    }

    if (stroke.enabled && stroke.width > 0 && stroke.color.a > 0) {
        Gdiplus::Pen pen(
            toGdiColor(stroke.color, impl_->opacity),
            stroke.width
        );
        pen.SetLineCap(toGdiCap(stroke.cap), toGdiCap(stroke.cap), Gdiplus::DashCapFlat);
        pen.SetLineJoin(toGdiJoin(stroke.join));
        impl_->graphics->DrawPath(&pen, &path);
    }
}

void Canvas::drawText(const TextRun& run) {
    if (!impl_ || !impl_->graphics)
        return;

    impl_->graphics->Flush(Gdiplus::FlushIntentionSync);
    TextRun shifted = run;
    shifted.x += impl_->offsetX;
    shifted.y += impl_->offsetY;
    drawRun(*this, shifted);
}

HDC Canvas::hdc() const {
    if (!impl_ || !impl_->graphics)
        return nullptr;

    impl_->graphics->Flush(Gdiplus::FlushIntentionSync);
    return impl_->hdc;
}

void Canvas::pushTranslate(float dx, float dy) {
    if (!impl_)
        return;

    impl_->translations.push({ impl_->offsetX, impl_->offsetY });
    impl_->offsetX += dx;
    impl_->offsetY += dy;
}

void Canvas::popTranslate() {
    if (!impl_ || impl_->translations.empty())
        return;

    impl_->offsetX = impl_->translations.top().first;
    impl_->offsetY = impl_->translations.top().second;
    impl_->translations.pop();
}

void Canvas::setOpacity(float a) {
    if (!impl_)
        return;

    impl_->opacity = std::max(0.f, std::min(1.f, a));
}

bool initGraphics() {
    if (g_graphicsUsers++ > 0)
        return true;

    Gdiplus::GdiplusStartupInput input;
    Gdiplus::Status status = Gdiplus::GdiplusStartup(
        &g_gdiplusToken, &input, nullptr
    );

    if (status != Gdiplus::Ok) {
        g_gdiplusToken = 0;
        return false;
    }

    return true;
}

void shutdownGraphics() {
    if (g_graphicsUsers > 0)
        --g_graphicsUsers;

    if (g_graphicsUsers == 0 && g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

} // namespace ui
