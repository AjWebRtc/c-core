/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#include "pubnub_qt.h"

extern "C" {
#include "pubnub_ccore.h"
}

#include <QtNetwork>



pubnub_qt::pubnub_qt(QString pubkey, QString keysub) 
    : d_pubkey(pubkey.toLatin1())
    , d_keysub(keysub.toLatin1())
    , d_context(new pbcc_context)
    , d_http_code(0)
#ifdef QT_NO_SSL
    , d_origin("http://pubsub.pubnub.com")
    , d_ssl_opts(0)
#else
    , d_origin("https://pubsub.pubnub.com")
    , d_ssl_opts(useSSL)
#endif
    , d_transaction_timeout_duration_ms(10000)
    , d_transaction_timed_out(false)
    , d_transactionTimer(new QTimer(this))
{
    pbcc_init(d_context.data(), d_pubkey.data(), d_keysub.data());
    connect(&d_qnam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
    connect(d_transactionTimer, SIGNAL(timeout()), this, SLOT(transactionTimeout()));
}


pubnub_qt::~pubnub_qt()
{
    pbcc_deinit(d_context.data());
}


pubnub_res pubnub_qt::startRequest(pubnub_res result, pubnub_trans transaction)
{
    if (PNR_STARTED == result) {
        QUrl url(d_origin + QString::fromLatin1(d_context->http_buf, d_context->http_buf_len));
        d_trans = transaction;

        QNetworkReply *p = d_reply.take();
        if (p) {
            p->deleteLater();
        }
        d_transaction_timed_out = false;
        d_reply.reset(d_qnam.get(QNetworkRequest(url)));
        connect(d_reply.data(), SIGNAL(finished()), this, SLOT(httpFinished()));
        d_transactionTimer->start(d_transaction_timeout_duration_ms);
    }
    return result;
}


void pubnub_qt::set_uuid(QString const &uuid)
{
    d_uuid = uuid.toLatin1();
    pbcc_set_uuid(d_context.data(), d_uuid.data());
}


void pubnub_qt::set_auth(QString const &auth)
{
    d_auth = auth.toLatin1();
    pbcc_set_auth(d_context.data(), d_auth.data());
}

QString pubnub_qt::get() const
{
    return pbcc_get_msg(d_context.data());
}


QStringList pubnub_qt::get_all() const
{
    QStringList all;
    while (char const *msg = pbcc_get_msg(d_context.data())) {
        if (0 == msg) {
            break;
        }
        all.push_back(msg);
    }
    return all;
}


QString pubnub_qt::get_channel() const
{
    return pbcc_get_channel(d_context.data());
}


QStringList pubnub_qt::get_all_channels() const
{
    QStringList all;
    while (char const *msg = pbcc_get_channel(d_context.data())) {
        if (0 == msg) {
            break;
        }
        all.push_back(msg);
    }
    return all;
}


void pubnub_qt::cancel()
{
    if (d_reply) {
        d_reply->abort();
    }
}


pubnub_res pubnub_qt::publish(QString const &channel, QString const &message)
{
    return startRequest(
        pbcc_publish_prep(
            d_context.data(), 
            channel.toLatin1().data(), 
            message.toLatin1().data(), 
            false, 
            false
            ), PBTT_PUBLISH
        );
}


pubnub_res pubnub_qt::publishv2(QString const &channel, QString const &message, pubv2_opts options)
{
    return startRequest(
        pbcc_publish_prep(
            d_context.data(), 
            channel.toLatin1().data(), 
            message.toLatin1().data(), 
            options & store_in_history, 
            options & eat_after_reading
            ), PBTT_PUBLISH
        );
}


pubnub_res pubnub_qt::subscribe(QString const &channel, QString const &channel_group)
{
    return startRequest(
        pbcc_subscribe_prep(
            d_context.data(), 
            channel.isEmpty() ? 0 : channel.toLatin1().data(), 
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            0
            ), PBTT_SUBSCRIBE
        );
}


pubnub_res pubnub_qt::leave(QString const &channel, QString const &channel_group)
{
    return startRequest(
        pbcc_leave_prep(
            d_context.data(), 
            channel.isEmpty() ? 0 : channel.toLatin1().data(), 
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data()
            ), PBTT_LEAVE
        );
}


pubnub_res pubnub_qt::time()
{
    return startRequest(pbcc_time_prep(d_context.data()), PBTT_TIME);
}


pubnub_res pubnub_qt::history(QString const &channel, unsigned count, bool include_token)
{
    return startRequest(
        pbcc_history_prep(
            d_context.data(),
            channel.isEmpty() ? 0 : channel.toLatin1().data(),
            count,
            include_token,
            pbccNotSet,
            pbccNotSet,
            0,
            0
            ), PBTT_HISTORY
        );
}


pubnub_res pubnub_qt::history(QString const &channel, unsigned count, bool include_token, QString const& start, bool reverse, QString const& end, bool string_token)
{
    return startRequest(
        pbcc_history_prep(
            d_context.data(),
            channel.isEmpty() ? 0 : channel.toLatin1().data(),
            count,
            include_token,
            string_token ? pbccTrue : pbccFalse,
            reverse ? pbccTrue : pbccFalse,
            start.isEmpty() ? 0 : start.toLatin1().data(), 
            end.isEmpty() ? 0 : end.toLatin1().data()
            ), PBTT_HISTORY
        );
}


pubnub_res pubnub_qt::here_now(QString const &channel, QString const &channel_group)
{
    return startRequest(
        pbcc_here_now_prep(
            d_context.data(), 
            channel.isEmpty() ? 0 : channel.toLatin1().data(), 
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            pbccNotSet,
            pbccNotSet
            ), PBTT_HERENOW
        );
}


pubnub_res pubnub_qt::global_here_now()
{
    return startRequest(pbcc_here_now_prep(d_context.data(), 0, 0, pbccNotSet, pbccNotSet), PBTT_GLOBAL_HERENOW);
}


pubnub_res pubnub_qt::where_now(QString const &uuid)
{
    return startRequest(
        pbcc_where_now_prep(
            d_context.data(), 
            uuid.isEmpty() ? d_uuid.data() : uuid.toLatin1().data()
            ), PBTT_WHERENOW
        );
}


pubnub_res pubnub_qt::set_state(QString const &channel, QString const& channel_group, QString const &uuid, QString const &state)
{
    return startRequest(
        pbcc_set_state_prep(
            d_context.data(),
            channel.isEmpty() ? 0 : channel.toLatin1().data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            uuid.isEmpty() ? d_uuid.data() : uuid.toLatin1().data(),
            state.toLatin1().data()
            ), PBTT_SET_STATE
        );
}


pubnub_res pubnub_qt::state_get(QString const &channel, QString const& channel_group, QString const &uuid)
{
    return startRequest(
        pbcc_state_get_prep(
            d_context.data(),
            channel.isEmpty() ? 0 : channel.toLatin1().data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            uuid.isEmpty() ? d_uuid.data() : uuid.toLatin1().data()
            ), PBTT_STATE_GET
        );
}


pubnub_res pubnub_qt::remove_channel_group(QString const& channel_group)
{
    return startRequest(
        pbcc_remove_channel_group_prep(
            d_context.data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data()
            ), PBTT_REMOVE_CHANNEL_GROUP
        );
}


pubnub_res pubnub_qt::remove_channel_from_group(QString const &channel, QString const& channel_group)
{
    return startRequest(
        pbcc_channel_registry_prep(
            d_context.data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            "remove",
            channel.isEmpty() ? 0 : channel.toLatin1().data()
            ), PBTT_REMOVE_CHANNEL_FROM_GROUP
        );
}


pubnub_res pubnub_qt::add_channel_to_group(QString const &channel, QString const& channel_group)
{
    return startRequest(
        pbcc_channel_registry_prep(
            d_context.data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            "add",
            channel.isEmpty() ? 0 : channel.toLatin1().data()
            ), PBTT_ADD_CHANNEL_TO_GROUP
        );
}



pubnub_res pubnub_qt::list_channel_group(QString const& channel_group)
{
    return startRequest(
        pbcc_channel_registry_prep(
            d_context.data(),
            channel_group.isEmpty() ? 0 : channel_group.toLatin1().data(),
            0,
            0
            ), PBTT_LIST_CHANNEL_GROUP
        );
}


int pubnub_qt::last_http_code() const
{
    return d_http_code;
}


QString pubnub_qt::last_publish_result() const
{
    if (PUBNUB_DYNAMIC_REPLY_BUFFER && (NULL == d_context->http_reply)) {
        return "";
    }
    if ((d_trans != PBTT_PUBLISH) || (d_context->http_reply[0] == '\0')) {
        return "";
    }

    char *end;
    strtol(d_context->http_reply + 1, &end, 10);
    return end + 1;
}


pubnub_publish_res pubnub_qt::parse_last_publish_result()
{
    QString toParse = last_publish_result();
    return pubnub_parse_publish_result(toParse.toLatin1());
}


QString pubnub_qt::last_time_token() const
{
    return d_context->timetoken;
}


void pubnub_qt::set_ssl_options(ssl_opts options)
{
    if (options & useSSL) {
        if (d_origin.startsWith("http:")) {
            d_origin.replace(0, 5, "https:");
        }
    }
    else {
        if (d_origin.startsWith("https:")) {
            d_origin.replace(0, 6, "http:");
        }
    }
    d_ssl_opts = options;
}


void pubnub_qt::set_origin(QString const& origin)
{
    d_origin = origin;
    if (!origin.startsWith("http:") && !origin.startsWith("https:")) {
        d_origin.prepend("http://");
        set_ssl_options(d_ssl_opts);
    }
}


pubnub_res pubnub_qt::finish(QByteArray const &data, int http_code)
{
    pubnub_res pbres = PNR_OK;
    if (PUBNUB_DYNAMIC_REPLY_BUFFER) {
        pbcc_realloc_reply_buffer(d_context.data(), data.size());
        memcpy(d_context->http_reply, data.data(), data.size());
        d_context->http_buf_len = data.size();
        d_context->http_reply[data.size()] = '\0';
    }
    else {
        if ((unsigned)data.size() >= sizeof d_context->http_reply) {
            return PNR_REPLY_TOO_BIG;
        }
        d_context->http_buf_len = data.size();
        memcpy(d_context->http_reply, data.data(), data.size());
        d_context->http_reply[d_context->http_buf_len] = '\0';
    }

    qDebug() << "finish('" << d_context->http_reply << "')";
    
    switch (d_trans) {
    case PBTT_SUBSCRIBE:
        if (pbcc_parse_subscribe_response(d_context.data()) != 0) {
            pbres = PNR_FORMAT_ERROR;
        }
        break;
    case PBTT_PUBLISH:
        pbres = pbcc_parse_publish_response(d_context.data());
        break;
    case PBTT_TIME:
        if (pbcc_parse_time_response(d_context.data()) != 0) {
            pbres = PNR_FORMAT_ERROR;
        }
        break;
    case PBTT_HISTORY:
        if (pbcc_parse_history_response(d_context.data()) != 0) {
            pbres = PNR_FORMAT_ERROR;
        }
        break;
    case PBTT_LEAVE:
    case PBTT_HERENOW:
    case PBTT_GLOBAL_HERENOW:
    case PBTT_WHERENOW:
    case PBTT_SET_STATE:
    case PBTT_STATE_GET:
        if (pbcc_parse_presence_response(d_context.data()) != 0) {
            pbres = PNR_FORMAT_ERROR;
        }
        break;
    case PBTT_REMOVE_CHANNEL_GROUP:
    case PBTT_REMOVE_CHANNEL_FROM_GROUP:
    case PBTT_ADD_CHANNEL_TO_GROUP:
    case PBTT_LIST_CHANNEL_GROUP:
        pbres = pbcc_parse_channel_registry_response(d_context.data());
        break;
    default:
        break;
    }

    QVariant statusCode = d_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    d_http_code = statusCode.isValid() ? statusCode.toInt() : 0;

    if ((PNR_OK == pbres) && (http_code != 0) && (http_code / 100 != 2)) {
        return PNR_HTTP_ERROR;
    }

    return pbres;
}


void pubnub_qt::transactionTimeout()
{
    if (d_reply) {
        d_transaction_timed_out = true;
        d_reply->abort();
    }
}


void pubnub_qt::httpFinished()
{
    d_transactionTimer->stop();

    QNetworkReply::NetworkError error = d_reply->error();
    if (error) {
        qDebug() << "error: " << d_reply->error() << ", string: " << d_reply->errorString();
        d_context->http_buf_len = 0;
        if (PUBNUB_DYNAMIC_REPLY_BUFFER) {
            d_context->http_reply = NULL;
        }
        else {
            d_context->http_reply[0] = '\0';
        }
        switch (error) {
        case QNetworkReply::OperationCanceledError:
            if (d_transaction_timed_out) {
                emit outcome(PNR_TIMEOUT);
            }
            else {
                emit outcome(PNR_CANCELLED);
            }
            return;
        case QNetworkReply::TimeoutError:
            emit outcome(PNR_CONNECTION_TIMEOUT);
            return;
        case QNetworkReply::HostNotFoundError:
            emit outcome(PNR_ADDR_RESOLUTION_FAILED);
            return;
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::ProtocolUnknownError:
            emit outcome(PNR_CONNECT_FAILED);
            return;
        }
    }

    emit outcome(finish(d_reply->readAll(), d_reply->error()));
}


void pubnub_qt::sslErrors(QNetworkReply* reply,const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += ", ";
        errorString += error.errorString();
    }

    qDebug() << "SSL error: " << errorString;

    if (d_ssl_opts & ignoreSecureConnectionRequirement) {
        reply->ignoreSslErrors();
    }
}


extern "C" char const *pubnub_sdk_name() { return "Qt5"; }

extern "C" char const *pubnub_uname() { return "Qt5%2F2.1.0"; }

extern "C" char const *pubnub_version() { return "2.2.0"; }

