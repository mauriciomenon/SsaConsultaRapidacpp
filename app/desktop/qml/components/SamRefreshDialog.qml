pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root

    required property var workflowViewModel
    readonly property int availableWidth: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 900
    readonly property int availableHeight: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 700

    title: "Atualizacao SAM"
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: Math.min(620, availableWidth)
    minimumHeight: Math.min(560, availableHeight)
    width: Math.min(760, availableWidth)
    height: Math.min(640, availableHeight)

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Consulta REST por setor usando o projeto scrap_report local."
                color: Theme.mutedText
                wrapMode: Text.WordWrap
            }

            AppCheckBox {
                text: "Habilitar atualizacao SAM"
                checked: root.workflowViewModel.samRefreshEnabled
                onToggled: root.workflowViewModel.samRefreshEnabled = checked
            }
            AppCheckBox {
                text: "Executar automaticamente"
                enabled: root.workflowViewModel.samRefreshEnabled
                checked: root.workflowViewModel.samAutoRefreshEnabled
                onToggled: root.workflowViewModel.samAutoRefreshEnabled = checked
            }

            GridLayout {
                objectName: "samRefreshSettingsGrid"
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Label {
                    text: "Projeto scrap_report"
                    color: Theme.text
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: root.workflowViewModel.samScrapReportRoot
                    placeholderText: "/caminho/para/scrap_report"
                    onEditingFinished: root.workflowViewModel.samScrapReportRoot = text
                }

                Label {
                    text: "Arquivo CA"
                    color: Theme.text
                }
                AppTextField {
                    objectName: "samBaseUrlField"
                    Layout.fillWidth: true
                    text: root.workflowViewModel.samCaFile
                    placeholderText: "/caminho/para/itaipu.pem"
                    onEditingFinished: root.workflowViewModel.samCaFile = text
                }

                Label {
                    text: "URL base HTTPS"
                    color: Theme.text
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: root.workflowViewModel.samBaseUrl
                    placeholderText: "https://servidor/SAM/rest"
                    onEditingFinished: root.workflowViewModel.samBaseUrl = text
                }

                Label {
                    text: "Setores executores"
                    color: Theme.text
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: root.workflowViewModel.samExecutorSectors
                    placeholderText: "IEE3,MEL4"
                    onEditingFinished: root.workflowViewModel.samExecutorSectors = text
                }

                Label {
                    text: "Escopo"
                    color: Theme.text
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: root.workflowViewModel.samScope
                    readOnly: true
                }

                Label {
                    text: "Intervalo (minutos)"
                    color: Theme.text
                }
                AppSpinBox {
                    objectName: "samIntervalSpinBox"
                    from: 1
                    to: 30000
                    value: root.workflowViewModel.samIntervalMinutes
                    Layout.preferredWidth: 140
                    onValueModified: root.workflowViewModel.samIntervalMinutes = value
                }
            }

            Label {
                Layout.fillWidth: true
                text: "A atualizacao fica desabilitada por padrao e nao armazena senha, token ou segredo."
                color: Theme.mutedText
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gap

                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Atualizar agora"
                    enabled: root.workflowViewModel.samRefreshEnabled && !root.workflowViewModel.running
                    implicitWidth: 140
                    onClicked: root.workflowViewModel.refreshSamNow()
                }
                ActionButton {
                    text: "Fechar"
                    implicitWidth: 100
                    onClicked: root.close()
                }
            }
        }
    }
}
