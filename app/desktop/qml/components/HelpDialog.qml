pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root

    readonly property string generalSearchHelp: "Busca geral\n\nVirgula combina termos com E. Cada termo pode corresponder a qualquer coluna configurada para busca.\n!termo exclui, ^termo exige inicio, termo$ ou $termo exige final e =termo exige igualdade.\n~expressao aplica um padrao seguro ao campo inteiro; o ponto representa um caractere."
    readonly property string columnFilterHelp: "Filtros por coluna\n\nVirgula combina termos positivos com OU dentro da mesma coluna. Termos negativos removem correspondencias."
    readonly property string advancedFilterHelp: "Filtros avancados\n\nUse semana e ano, derivacao, somente reprogramadas e textos avancados. A consulta e executada no banco e mostra somente a pagina atual."
    readonly property string historyHelp: "Historico\n\nDesfazer restaura apenas as condicoes de filtro. Os resultados sao consultados novamente."
    readonly property string helpText: [root.generalSearchHelp, root.columnFilterHelp, root.advancedFilterHelp, root.historyHelp].join("\n\n")

    title: "Ajuda"
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: Math.min(560, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(420, Screen.desktopAvailableHeight)
    width: Math.min(760, Screen.desktopAvailableWidth)
    height: Math.min(620, Screen.desktopAvailableHeight)

    function open() {
        root.show();
        root.raise();
        root.requestActivate();
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: Theme.cardGap

            Label {
                text: "Como usar busca e filtros"
                color: Theme.text
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    text: root.helpText
                    color: Theme.text
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    background: Rectangle {
                        color: Theme.surface
                        border.color: Theme.borderSoft
                        radius: Theme.radiusSoft
                    }
                }
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text: "Fechar"
                onClicked: root.close()
            }
        }
    }
}
