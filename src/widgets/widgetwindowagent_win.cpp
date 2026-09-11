// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#include "widgetwindowagent_p.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>

#include <QtCore/private/qcoreapplication_p.h>

#include <QWKCore/qwindowkit_windows.h>
#include <QWKCore/private/qwkglobal_p.h>
#include <QWKCore/private/windows10borderhandler_p.h>

namespace QWK {

#if QWINDOWKIT_CONFIG(ENABLE_WINDOWS_SYSTEM_BORDERS)
    // On Windows 10 the system top border is removed together with the window frame, so it has to
    // be drawn manually, exactly like the Qt Quick implementation does (see BorderItem in
    // quickwindowagent_win.cpp).
    //
    // The previous implementation painted it with raw GDI (FillRect() on the window DC) *after*
    // Qt had already flushed its backing store to the screen. Because that rectangle lives outside
    // of Qt's backing store, every repaint left the border area in an inconsistent state for at
    // least one compositor frame: Qt flushes the client content, GDI paints over the same pixels,
    // and whichever of the two DWM happens to pick up becomes visible - which shows up as the top
    // border blinking on every repaint.
    //
    // Painting the border from a child widget instead keeps it inside the very same backing store
    // as the rest of the user interface, so it is uploaded and composited atomically with
    // everything else and therefore can never flicker.
    class WidgetBorderWidget : public QWidget {
    public:
        inline WidgetBorderWidget(QWidget *parent, Windows10BorderHandler *handler)
            : QWidget(parent), handler(handler) {
            // The border is pure decoration: it must never take part in hit testing, mouse
            // handling or focus handling.
            setAttribute(Qt::WA_TransparentForMouseEvents);
            setAttribute(Qt::WA_NoSystemBackground);
            setFocusPolicy(Qt::NoFocus);
        }

        inline void syncGeometry() {
            const int thickness = qMax(1, handler->borderThickness());
            // Keep it pinned to the top edge of the client area, which is exactly where the DWM
            // extended frame draws the (now removed) system border.
            const QRect target(0, 0, parentWidget()->width(), thickness);
            const bool targetVisible = handler->isNormalWindow();

            // Only touch the widget when the result actually changed. Re-applying an unchanged
            // geometry / visibility would invalidate the parent's dirty opaque region and
            // schedule a repaint the window does not need, which is perceived as the user
            // interface refreshing late.
            if (target == currentGeometry && targetVisible == currentVisible) {
                return;
            }
            currentGeometry = target;
            currentVisible = targetVisible;

            setGeometry(target);
            setVisible(targetVisible);
            raise();  // Make sure it always stays above the title bar / ribbon.
        }

    protected:
        inline void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            handler->drawBorderEmulated(&painter, QRect(QPoint(0, 0), size()));
        }

    private:
        Windows10BorderHandler *handler;

        // Cached state, so that syncGeometry() is a cheap no-op unless something changed.
        QRect currentGeometry;
        bool currentVisible = false;
    };

    class WidgetBorderHandler : public QObject, public Windows10BorderHandler {
    public:
        explicit WidgetBorderHandler(QWidget *widget, AbstractWindowContext *ctx,
                                     QObject *parent = nullptr)
            : QObject(parent), Windows10BorderHandler(ctx), widget(widget) {
            widget->installEventFilter(this);
            borderWidget = new WidgetBorderWidget(widget, this);

            // First update
            if (ctx->windowId()) {
                setupNecessaryAttributes();
            }
            WidgetBorderHandler::updateGeometry();
        }

        void updateGeometry() override {
            borderWidget->syncGeometry();
        }

        bool isWindowActive() const override {
            return widget->isActiveWindow();
        }

    protected:
        // NOTE: sharedEventFilter() is deliberately not overridden. The base class only needs it
        // for WinIdChange, and every geometry relevant event below is delivered to the host
        // widget itself, so handling them in a single place avoids syncing twice per event.
        bool eventFilter(QObject *obj, QEvent *event) override {
            Q_UNUSED(obj)

            switch (event->type()) {
                case QEvent::Resize:
                case QEvent::Show:
                case QEvent::WindowStateChange: {
                    updateGeometry();
                    break;
                }

                case QEvent::WindowActivate:
                case QEvent::WindowDeactivate: {
                    // The border colour depends on the activation state of the window, and the
                    // context moves the native extended frame on (de)activation, so the client
                    // area right below it has to be refreshed as well. Repainting through the
                    // backing store keeps this flicker free.
                    widget->update();
                    borderWidget->update();
                    break;
                }

                default:
                    break;
            }
            return false;
        }

    private:
        QWidget *widget;
        WidgetBorderWidget *borderWidget;
    };

    void WidgetWindowAgentPrivate::setupWindows10BorderWorkaround() {
        // Install painting hook
        auto ctx = context.get();
        if (ctx->windowAttribute(QStringLiteral("win10-border-needed")).toBool()) {
            borderHandler = std::make_unique<WidgetBorderHandler>(hostWidget, ctx);
        }
    }
#endif

}
