pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

ComboBox {
    id: root

    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 10
    rightPadding: 22
    font.family: Theme.fontFamily
    font.pointSize: Theme.fontPointSizeBody

    function clampedPopupX(popupWidth) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return 0;
        const origin = root.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        return Theme.clampedPopupX(boundsWidth, origin.x + root.width, popupWidth);
    }

    function clampedPopupY(popupHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return root.height + Theme.shortcutGap;
        const origin = root.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        return Theme.clampedPopupY(boundsHeight, origin.y, root.height, popupHeight);
    }

    delegate: ItemDelegate {
        id: delegateRoot
        objectName: "appComboBoxDelegate"
        required property int index
        required property var modelData
        width: root.popup.width
        height: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
        text: root.textRole.length > 0 && modelData[root.textRole] !== undefined ? modelData[root.textRole] : modelData
        font: root.font
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: delegateRoot.text
            textFormat: Text.PlainText
            color: delegateRoot.highlighted ? Theme.readableText(Theme.accentSoft, Theme.accentStrong) : Theme.readableText(Theme.panelRaised, Theme.text)
            font: delegateRoot.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: delegateRoot.highlighted ? Theme.accentSoft : Theme.panelRaised
        }
    }

    indicator: Canvas {
        id: comboIndicator
        width: 8
        height: 5
        x: root.width - width - 9
        y: Math.round((root.height - height) / 2)

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(width, 0);
            ctx.lineTo(width / 2, height);
            ctx.closePath();
            ctx.fillStyle = root.enabled ? Theme.mutedText : Theme.border;
            ctx.fill();
        }

        Connections {
            target: Theme
            function onThemeNameChanged() {
                comboIndicator.requestPaint();
            }
        }
        Connections {
            target: root
            function onEnabledChanged() {
                comboIndicator.requestPaint();
            }
        }
    }

    contentItem: Text {
        leftPadding: root.leftPadding
        rightPadding: root.rightPadding
        text: root.displayText
        textFormat: Text.PlainText
        color: root.enabled ? Theme.text : Theme.mutedText
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: root.enabled ? Theme.panelRaised : Theme.rowAlt
        border.color: root.activeFocus ? Theme.accent : Theme.border
        radius: Theme.radius
    }

    // Resolve X/Y/height together so the popup opens directly below the
    // trigger when possible (shrinking height to fit), and only clamps Y up
    // when opening below cannot fit at all.
    function resolvePopupGeometry(preferredWidth, preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return ({
                    x: 0,
                    y: root.height + Theme.shortcutGap,
                    h: preferredHeight
                });
        const origin = root.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        const originRightX = origin.x + root.width;
        const originBottomY = origin.y + root.height;
        const x = Theme.clampedPopupX(boundsWidth, originRightX, preferredWidth);
        const belowHeight = Theme.clampedPopupHeightBelow(boundsHeight, originBottomY + Theme.shortcutGap, preferredHeight, 0);
        if (belowHeight > 0) {
            return ({
                    x: x,
                    y: originBottomY + Theme.shortcutGap,
                    h: belowHeight
                });
        }
        return ({
                x: x,
                y: Theme.clampedPopupY(boundsHeight, origin.y, root.height, preferredHeight),
                h: Theme.clampedPopupHeight(boundsHeight, preferredHeight, 0)
            });
    }

    popup: Popup {
        id: comboPopup
        objectName: "appComboBoxPopup"
        parent: Overlay.overlay
        x: root.clampedPopupX(width)
        y: root.clampedPopupY(height)
        width: root.width
        implicitHeight: contentListView.implicitHeight
        padding: 1

        function updatePopupPosition() {
            const g = root.resolvePopupGeometry(width, contentListView.implicitHeight > 0 ? contentListView.implicitHeight : height);
            x = g.x;
            y = g.y;
            height = g.h;
        }

        onAboutToShow: updatePopupPosition()
        onOpened: updatePopupPosition()
        onWidthChanged: {
            if (visible)
                updatePopupPosition();
        }
        onHeightChanged: {
            if (visible)
                updatePopupPosition();
        }
        Connections {
            target: contentListView
            function onImplicitHeightChanged() {
                if (root.popup.visible)
                    comboPopup.updatePopupPosition();
            }
        }

        contentItem: ListView {
            id: contentListView
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollBar.vertical: ScrollBar {}
        }

        background: Rectangle {
            color: Theme.panelRaised
            border.color: Theme.border
            radius: Theme.radius
        }
    }
}
