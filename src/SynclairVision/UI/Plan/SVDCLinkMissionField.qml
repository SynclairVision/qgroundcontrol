import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root

    property int decimals: -1
    required property string label
    property real maximum: Number.MAX_VALUE
    property real minimum: -Number.MAX_VALUE
    readonly property bool valid: field.acceptableInput
    required property real value

    signal edited(real value)

    spacing: ScreenTools.defaultFontPixelHeight * 0.2

    QGCLabel {
        Layout.fillWidth: true
        font.pointSize: ScreenTools.smallFontPointSize
        text: root.label
        wrapMode: Text.WordWrap
    }

    QGCTextField {
        id: field

        Layout.fillWidth: true
        numericValuesOnly: true

        validator: DoubleValidator {
            bottom: root.minimum
            decimals: root.decimals
            locale: "C"
            notation: DoubleValidator.ScientificNotation
            top: root.maximum
        }

        onEditingFinished: {
            if (!acceptableInput) {
                text = root.decimals < 0 ? root.value.toString() : root.value.toFixed(root.decimals);
            }
        }
        onTextEdited: {
            if (acceptableInput) {
                root.edited(Number(text));
            }
        }

        Binding {
            property: "text"
            restoreMode: Binding.RestoreNone
            target: field
            value: root.decimals < 0 ? root.value.toString() : root.value.toFixed(root.decimals)
            when: !field.activeFocus
        }
    }
}
