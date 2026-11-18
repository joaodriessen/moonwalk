#pragma once
#include <QByteArray>
#include <QMutex>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <stdexcept>
class NvComputer;
struct RuntimeTransport {
    inline static std::atomic<int> capabilities {0}, writes {0}, lastTarget {0};
    inline static std::atomic<bool> hold {false}, unsupported {false}, reject {false};
    static void reset() { capabilities=0; writes=0; lastTarget=0; hold=false; unsupported=false; reject=false; }
};
class NvHTTP {
public:
    enum NvLogLevel { NVLL_ERROR };
    struct RawResponse { int httpStatusCode; QByteArray body; };
    struct CancelToken { std::atomic<bool> cancelled{false}; void cancel(){cancelled=true;} bool isCancelled()const{return cancelled;} };
    QUrl m_BaseUrlHttps;
    explicit NvHTTP(NvComputer*) {}
    void setCancelToken(CancelToken*) {}
    RawResponse openConnectionRaw(QUrl, QString command, QString args, int, QByteArray, NvLogLevel, CancelToken* cancel, qint64) {
        while(RuntimeTransport::hold && !cancel->isCancelled()) QThread::msleep(1);
        if(cancel->isCancelled()) throw std::runtime_error("cancelled");
        if(command == "api/abr/capabilities") {
            ++RuntimeTransport::capabilities;
            return {200, RuntimeTransport::unsupported ? QByteArray("{}") : QByteArray("{\"supported\":false,\"version\":1,\"features\":[\"runtime_bitrate\"]}")};
        }
        const int target=args.mid(QString("bitrate=").size()).toInt();
        ++RuntimeTransport::writes; RuntimeTransport::lastTarget=target;
        if(RuntimeTransport::reject) return {200,"<root status_code=\"403\"><bitrate>0</bitrate></root>"};
        return {200,"<root status_code=\"200\"><bitrate>" + QByteArray::number(target) + "</bitrate></root>"};
    }
};
