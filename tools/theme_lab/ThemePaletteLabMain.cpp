#include "ThemePaletteLab.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

namespace {

    int reportError(const QString& message, const int exitCode) {
        QTextStream(stderr) << "theme-lab: " << message << '\n';
        return exitCode;
    }

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ssa_theme_lab"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Map and rank offline Tinted Base16 schemes."));
    parser.addHelpOption();
    const QCommandLineOption inputOption(QStringList{QStringLiteral("input")},
                                         QStringLiteral("Builder JSON directory."),
                                         QStringLiteral("directory"));
    const QCommandLineOption outputOption(QStringList{QStringLiteral("output")},
                                          QStringLiteral("Analysis report path."),
                                          QStringLiteral("file"));
    const QCommandLineOption limitOption(QStringList{QStringLiteral("limit")},
                                         QStringLiteral("Maximum shortlist size."),
                                         QStringLiteral("count"), QStringLiteral("24"));
    const QCommandLineOption sourceCommitOption(
        QStringList{QStringLiteral("source-commit")},
        QStringLiteral("Pinned tinted-theming/schemes commit."), QStringLiteral("sha"));
    const QCommandLineOption sourceRepositoryOption(
        QStringList{QStringLiteral("source-repository")},
        QStringLiteral("Source repository recorded in the report."), QStringLiteral("url"),
        QStringLiteral("https://github.com/tinted-theming/schemes"));
    parser.addOptions(
        {inputOption, outputOption, limitOption, sourceCommitOption, sourceRepositoryOption});
    parser.process(application);

    if (!parser.isSet(inputOption) || !parser.isSet(outputOption) ||
        !parser.isSet(sourceCommitOption)) {
        return reportError(QStringLiteral("--input, --output and --source-commit are required"), 2);
    }
    bool limitValid = false;
    const int limit = parser.value(limitOption).toInt(&limitValid);
    if (!limitValid || limit < 1 || limit > 100) {
        return reportError(QStringLiteral("--limit must be between 1 and 100"), 2);
    }

    QStringList catalogErrors;
    const QVector<ssa::theme_lab::Base16Scheme> catalog =
        ssa::theme_lab::readCatalog(parser.value(inputOption), catalogErrors);
    if (!catalogErrors.isEmpty()) {
        return reportError(catalogErrors.join(QLatin1Char('\n')), 2);
    }
    if (catalog.isEmpty()) {
        return reportError(QStringLiteral("input catalog is empty"), 2);
    }

    QJsonObject report = ssa::theme_lab::analyzeCatalog(catalog, limit);
    report.insert(QStringLiteral("source"),
                  QJsonObject{
                      {QStringLiteral("repository"), parser.value(sourceRepositoryOption)},
                      {QStringLiteral("commit"), parser.value(sourceCommitOption)},
                  });
    const QByteArray document = QJsonDocument(report).toJson(QJsonDocument::Indented);
    QSaveFile output(parser.value(outputOption));
    if (!output.open(QIODevice::WriteOnly)) {
        return reportError(output.errorString(), 3);
    }
    if (output.write(document) != document.size()) {
        output.cancelWriting();
        return reportError(QStringLiteral("short write while saving report"), 3);
    }
    if (!output.commit()) {
        return reportError(output.errorString(), 3);
    }

    QTextStream(stdout) << "THEME_LAB input=" << report.value(QStringLiteral("inputCount")).toInt()
                        << " accepted=" << report.value(QStringLiteral("acceptedCount")).toInt()
                        << " rejected=" << report.value(QStringLiteral("rejectedCount")).toInt()
                        << " shortlisted="
                        << report.value(QStringLiteral("shortlist")).toArray().size() << '\n';
    return 0;
}
