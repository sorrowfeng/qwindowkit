// Copyright (C) 2023-present Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef QWKGLOBAL_H
#define QWKGLOBAL_H

#include <functional>

#include <QtCore/QEvent>
#include <QtGui/QtEvents>

#ifndef QWK_CORE_EXPORT
#  ifdef QWK_CORE_STATIC
#    define QWK_CORE_EXPORT
#  else
#    ifdef QWK_CORE_LIBRARY
#      define QWK_CORE_EXPORT Q_DECL_EXPORT
#    else
#      define QWK_CORE_EXPORT Q_DECL_IMPORT
#    endif
#  endif
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
using QT_NATIVE_EVENT_RESULT_TYPE = qintptr;
using QT_ENTER_EVENT_TYPE = QEnterEvent;
#else
using QT_NATIVE_EVENT_RESULT_TYPE = long;
using QT_ENTER_EVENT_TYPE = QEvent;
#endif

#ifndef QWINDOWKIT_CONFIG
#  define QWINDOWKIT_CONFIG(feature) ((1 / QWINDOWKIT_##feature) == 1)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define QWINDOWKIT_PRINTF_FORMAT(fmtpos, attrpos)                                                \
      __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#  define QWINDOWKIT_PRINTF_FORMAT(fmtpos, attrpos)
#endif

namespace QWK {

    using ScreenRectCallback = std::function<QRect(const QSize &)>;

    // 告知 QWindowKit: 进程即将退出, 不要再把窗口消息转发给 Qt 平台层。
    //
    // 崩溃现场(2026-10, 上位机退出必崩):
    //   exit() -> LdrShutdownProcess -> RtlProcessFlsData -> KiUserCallbackDispatcher
    //     -> QWKHookedWndProc -> 转发给 Qt 的窗口过程 -> NULL 解引用 (c0000005)
    // 原因: 进程收尾阶段 Windows 仍会把挂起的窗口消息派发进来, 但此时 Qt 的窗口
    // 平台上下文(QWindowsContext 等)已经析构, 转发过去就是访问空指针。
    // 应用应在 QCoreApplication 析构阶段(窗口已销毁、进程尚未真正退出)通过
    // qAddPostRoutine() 调用本函数; 置位后钩子过程直接走 DefWindowProcW。
    QWK_CORE_EXPORT void setProcessShuttingDown(bool value = true);

}

#endif // QWKGLOBAL_H
