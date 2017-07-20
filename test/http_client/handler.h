#ifndef __HTTP_RESPONSE_HANDLER_H__
#define __HTTP_RESPONSE_HANDLER_H__

class HttpResponseHandler
{
public:
    virtual ~HttpResponseHandler() {}

    // ���ִ���ʱ�ص�
    virtual void onError()
    {
    }

    // ״̬�н��ռ�������Ϻ�ص�
    virtual void onStatus(
        int code)
    {
    }

    // һ��HTTP Header���ռ�������Ϻ�ص�
    virtual void onHeader(
        const char *name, 
        unsigned int nameLen,
        const char *value,
        unsigned int valueLen)
    {
    }

    // Headers����ʱ�ص�
    virtual void onHeaderEnd()
    {
    }

    // �յ�Body����ʱ�ص�
    virtual void onBody(
        const void *data,
        unsigned int len)
    {
    }

    // ����Response����ʱ�ص�
    virtual void onEnd()
    {
    }
};

#endif // !__HTTP_RESPONSE_HANDLER_H__
