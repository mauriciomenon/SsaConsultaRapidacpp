#include "presentation/CurrentWeekViewModel.h"

#include <QDate>
#include <QDateTime>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QTest>
#include <QTime>

namespace {

    class CurrentWeekViewModelTest final : public QObject {
        Q_OBJECT

      private slots:
        void value_property_has_notify_signal() {
            const QMetaProperty property =
                ssa::presentation::CurrentWeekViewModel::staticMetaObject.property(
                    ssa::presentation::CurrentWeekViewModel::staticMetaObject.indexOfProperty(
                        "value"));

            QVERIFY(property.isValid());
            QVERIFY(property.hasNotifySignal());
            QCOMPARE(property.notifySignal().name(), QByteArray("valueChanged"));
        }

        void refresh_updates_value_only_when_iso_week_changes() {
            QDateTime now(QDate(2024, 12, 29), QTime(12, 0));
            ssa::presentation::CurrentWeekViewModel model([&now] { return now; });
            QSignalSpy valueChanged(&model, &ssa::presentation::CurrentWeekViewModel::valueChanged);

            const QString firstValue = model.value();
            QVERIFY(QMetaObject::invokeMethod(&model, "refresh", Qt::DirectConnection));
            QCOMPARE(valueChanged.size(), 0);

            now = QDateTime(QDate(2024, 12, 30), QTime(12, 0));
            QVERIFY(QMetaObject::invokeMethod(&model, "refresh", Qt::DirectConnection));

            int isoYear = 0;
            const int isoWeek = now.date().weekNumber(&isoYear);
            QCOMPARE(model.value(), QString::number(isoYear * 100 + isoWeek));
            QVERIFY(model.value() != firstValue);
            QCOMPARE(valueChanged.size(), 1);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(CurrentWeekViewModelTest)

#include "CurrentWeekViewModelTest.moc"
