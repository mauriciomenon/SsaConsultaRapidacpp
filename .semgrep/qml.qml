import QtQuick

Item {
    function unsafeDynamicCode(url) {
        // ruleid: qml-no-direct-external-url
        Qt.openUrlExternally(url);
        // ruleid: qml-no-dynamic-qml-object
        Qt.createQmlObject("Item {}", this);
        // ruleid: qml-no-dynamic-component-url
        Qt.createComponent("Remote.qml");
        // ruleid: qml-no-eval
        eval("1 + 1");
    }

    Text {
        // ruleid: qml-no-rich-text
        textFormat: Text.RichText
    }

    // ruleid: qml-value-popup-must-stay-virtualized
    Repeater {
        model: 1
        delegate: Item {}
    }
}
