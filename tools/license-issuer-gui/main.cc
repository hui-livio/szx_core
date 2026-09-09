#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>

#include "src/IssuerGuiController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SZX"));
    QCoreApplication::setApplicationName(QStringLiteral("LicenseIssuer"));
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/SzxLicenseIssuer/app.svg")));
    QQmlApplicationEngine engine;
    IssuerGuiController controller;
    qmlRegisterSingletonInstance("SzxLicenseIssuerBackend", 1, 0, "IssuerController", &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("issuerController"), &controller);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("SzxLicenseIssuer", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
