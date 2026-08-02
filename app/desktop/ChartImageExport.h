#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

namespace ssa::app::desktop {

    class ChartImageExport : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON

      public:
        ChartImageExport() = default;
        explicit ChartImageExport(QObject* parent);

        static ChartImageExport* create(QQmlEngine* engine, QJSEngine* scriptEngine);

        Q_INVOKABLE bool grabItemToFile(QObject* itemObject, const QString& path) const;
        Q_INVOKABLE bool grabItemToSvgFile(QObject* itemObject, const QString& path) const;
    };

} // namespace ssa::app::desktop
