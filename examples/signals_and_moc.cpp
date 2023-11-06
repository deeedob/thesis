// signing_and_moc.cpp
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QTimer>
#include <QDebug>

// To use signals and slots, we must inherit from QObject.
class MyObject : public QObject
{
    Q_OBJECT  // Tell MOC to target this class
public:
    MyObject(QObject *parent = 0) : QObject(parent) {}
    void setData(qsizetype data)
    {
        if (data == mData) { // No change, don't emit signal
            qDebug() << "Rejected data: " << data;
            return;
        }
        mData = data;
        emit dataChanged(data);
    }
signals:
    void dataChanged(qsizetype data);
private:
    qsizetype mData;
};

void receiveOnFunction(qsizetype data)
{ qDebug() << "1. Received data on free function: " << data; }

class MyReceiver : public QObject
{
    Q_OBJECT
public slots:
    void receive(qsizetype data)
    { qDebug() << "2. Received data on member function: " << data; }
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    MyObject obj;
    MyReceiver receiver;

    QTimer timer; // Set up a timer to change the data every second
    timer.setInterval(1000);
    QObject::connect(&timer, &QTimer::timeout, [&obj]() {
        obj.setData(QRandomGenerator::global()->bounded(3));
    });
    timer.start();

    // Connect the signal to three different receivers.
    QObject::connect(&obj, &MyObject::dataChanged, &receiveOnFunction);
    QObject::connect(&obj, &MyObject::dataChanged, &receiver, &MyReceiver::receive);
    QObject::connect(&obj, &MyObject::dataChanged, [](qsizetype data) {
        qDebug() << "3. Received data on lambda function: " << data;
    });

    // Start the static event loop, which handles all published events.
    return QCoreApplication::exec();
}

#include "gen.moc"
