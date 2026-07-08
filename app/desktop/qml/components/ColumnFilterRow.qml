import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

RowLayout {
    id: root
    required property var row
    required property string placeholderText
    property bool clearPending: false
    signal filterSubmitted(string key, string value)
    signal filterCleared(string key)

    function focusInput() {
        rowFilterInput.forceActiveFocus();
        rowFilterInput.selectAll();
    }

    height: Theme.controlHeight
    spacing: Theme.gap

    onRowChanged: {
        if (root.row.value === undefined || root.row.value === null || root.row.value.length === 0)
            clearPending = false;
    }

    Label {
        Layout.preferredWidth: 120
        text: root.row.label
        color: Theme.text
        font.pixelSize: 13
        elide: Text.ElideRight
    }

    AppTextField {
        id: rowFilterInput
        Layout.fillWidth: true
        placeholderText: root.placeholderText
        font.pixelSize: 12
        onAccepted: {
            if (text.trim() !== "")
                root.filterSubmitted(root.row.key, text);
        }
    }

    Binding {
        target: rowFilterInput
        property: "text"
        value: root.row.value ?? ""
        when: !root.clearPending && (!rowFilterInput.activeFocus || root.row.value === undefined || root.row.value === null || root.row.value.length === 0)
        restoreMode: Binding.RestoreNone
    }

    ActionButton {
        text: "Aplicar"
        implicitWidth: 72
        enabled: rowFilterInput.text.trim() !== ""
        onClicked: {
            root.filterSubmitted(root.row.key, rowFilterInput.text);
        }
    }

    ActionButton {
        text: "Limpar"
        implicitWidth: 70
        onClicked: {
            root.clearPending = true;
            rowFilterInput.text = "";
            root.filterCleared(root.row.key);
        }
    }
}
