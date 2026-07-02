pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

ComboBox {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 22
    font.family: Theme.fontFamily
    font.pixelSize: 12

    delegate: ItemDelegate {
        id: delegateRoot
        required property int index
        required property var modelData
        width: root.popup.width
        height: Theme.controlHeight
        text: root.textRole.length > 0 && modelData[root.textRole] !== undefined ? modelData[root.textRole] : modelData
        font.family: Theme.fontFamily
        font.pixelSize: 12
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: delegateRoot.text
            color: delegateRoot.highlighted ? Theme.accentStrong : Theme.text
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

    popup: Popup {
        y: root.height + 1
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight, 260)
        padding: 1

        contentItem: ListView {
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
