#include "imagesource.h"
#include "../utils/ffmpegfactory.h"

#include <QFile>

extern "C" {
#include <libavutil/imgutils.h>
};

struct MemContext {
    const QByteArray &data;
    int pos = 0;
};

static int read_mem_cb(void *opaque, uint8_t *buf, int buf_size) {
    auto *ctx = static_cast<MemContext *>(opaque);
    int remaining = ctx->data.size() - ctx->pos;
    if (remaining <= 0) return AVERROR_EOF;
    int n = std::min(buf_size, remaining);
    memcpy(buf, ctx->data.constData() + ctx->pos, n);
    ctx->pos += n;
    return n;
}

static int64_t seek_mem_cb(void *opaque, int64_t offset, int whence) {
    auto *ctx = static_cast<MemContext *>(opaque);
    int64_t new_pos;
    switch (whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos = ctx->pos + offset; break;
        case SEEK_END: new_pos = ctx->data.size() + offset; break;
        case AVSEEK_SIZE: return ctx->data.size();
        default: return -1;
    }
    if (new_pos < 0 || new_pos > ctx->data.size()) return -1;
    ctx->pos = static_cast<int>(new_pos);
    return new_pos;
}

static QImage load_image_qt(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray data = file.readAll();
    QImage img;
    if (!img.loadFromData(data)) return {};
    return img;
}

static QImage load_image_ffmpeg(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray file_data = file.readAll();
    int buf_size = file_data.size();
    if (buf_size <= 0) return {};

    MemContext mem_ctx{file_data, 0};

    uint8_t *internal_buf = static_cast<uint8_t *>(av_malloc(4096));
    if (!internal_buf) return {};

    AVIOContext *avio = avio_alloc_context(internal_buf, 4096, 0, &mem_ctx,
                                           read_mem_cb, nullptr, seek_mem_cb);
    if (!avio) { av_free(internal_buf); return {}; }

    AVFormatContext *raw_ctx = avformat_alloc_context();
    if (!raw_ctx) { av_free(internal_buf); av_freep(&avio); return {}; }
    raw_ctx->pb = avio;

    if (avformat_open_input(&raw_ctx, "", nullptr, nullptr) < 0) return {};

    AVFormatContextPtr fmt_ctx(raw_ctx, [](AVFormatContext *ctx) {
        if (ctx) avformat_close_input(&ctx);
    });

    avformat_find_stream_info(fmt_ctx.get(), nullptr);

    int stream_idx = av_find_best_stream(fmt_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (stream_idx < 0) return {};

    const AVCodec *decoder = avcodec_find_decoder(
        fmt_ctx->streams[stream_idx]->codecpar->codec_id);
    if (!decoder) return {};

    AVCodecContextPtr dec_ctx(avcodec_alloc_context3(decoder));
    if (!dec_ctx) return {};
    avcodec_parameters_to_context(dec_ctx.get(), fmt_ctx->streams[stream_idx]->codecpar);
    if (avcodec_open2(dec_ctx.get(), decoder, nullptr) < 0) return {};

    AVPacketPtr pkt(av_packet_alloc(), [](AVPacket *p) { av_packet_free(&p); });
    AVFramePtr frame(av_frame_alloc(), [](AVFrame *f) { av_frame_free(&f); });
    if (!pkt || !frame) return {};

    QImage result;
    bool success = false;

    if (av_read_frame(fmt_ctx.get(), pkt.get()) >= 0 &&
        avcodec_send_packet(dec_ctx.get(), pkt.get()) >= 0 &&
        avcodec_receive_frame(dec_ctx.get(), frame.get()) >= 0)
    {
        SwsContextPtr sws(sws_getContext(
            frame->width, frame->height, dec_ctx->pix_fmt,
            frame->width, frame->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr));

        if (sws) {
            uint8_t *dst_data[4] = {nullptr};
            int dst_linesize[4] = {0};
            if (av_image_alloc(dst_data, dst_linesize, frame->width, frame->height,
                               AV_PIX_FMT_RGBA, 1) >= 0)
            {
                sws_scale(sws.get(), frame->data, frame->linesize, 0, frame->height,
                          dst_data, dst_linesize);

                result = QImage(dst_data[0], frame->width, frame->height,
                                dst_linesize[0], QImage::Format_RGBA8888,
                                [](void *ptr) { av_freep(&ptr); }, dst_data[0]);
                success = true;
            }
        }
    }

    if (!success) result = QImage();
    return result;
}

static QImage load_image_file(const QString &filePath) {
    QImage img = load_image_qt(filePath);
    if (!img.isNull()) return img;
    return load_image_ffmpeg(filePath);
}


ImageSource::ImageSource(const QString &file_path)
    : m_file_path(file_path) {
    load_image();
}

ImageSource::~ImageSource() = default;

bool ImageSource::load_image() {
    QImage img = load_image_file(m_file_path);
    if (img.isNull()) {
        base_width = 320.0f;
        base_height = 240.0f;
        lock_aspect_ratio = true;
        fixed_aspect_ratio = 4.0f / 3.0f;
        return false;
    }

    base_width = static_cast<float>(img.width());
    base_height = static_cast<float>(img.height());
    lock_aspect_ratio = true;
    fixed_aspect_ratio = base_width / base_height;
    return true;
}

void ImageSource::ensure_texture() {
    if (m_texture_id) return;

    if (!m_gl_initialized) {
        initializeOpenGLFunctions();
        m_gl_initialized = true;
    }

    QImage img = load_image_file(m_file_path);
    if (img.isNull()) return;

    QImage glImage = img.convertToFormat(QImage::Format_RGBA8888);

    m_tex_width = glImage.width();
    m_tex_height = glImage.height();

    this->glGenTextures(1, &m_texture_id);
    this->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    this->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_tex_width, m_tex_height, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, glImage.constBits());
}

void ImageSource::load_resources() {
    ensure_texture();
}

void ImageSource::unload_resources() {
    if (m_texture_id) {
        this->glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_tex_width = 0;
        m_tex_height = 0;
    }
}

void ImageSource::render() {
    ensure_texture();
    if (!m_texture_id) {
        glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(base_width, 0.0f);
        glVertex2f(base_width, base_height);
        glVertex2f(0.0f, base_height);
        glEnd();
        return;
    }

    this->glEnable(GL_TEXTURE_2D);
    this->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    this->glEnable(GL_BLEND);
    this->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(base_width, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(base_width, base_height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, base_height);
    glEnd();

    this->glDisable(GL_BLEND);
    this->glDisable(GL_TEXTURE_2D);
}
