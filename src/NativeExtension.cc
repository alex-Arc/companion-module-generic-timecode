#include <cstddef>
#include <cstdint>
#include <nan.h>

#include "libltc/src/decoder.h"
#include "libltc/src/encoder.h"
#include "libltc/src/ltc.h"

// TODO: rename file

// From ltc-tool https://github.com/x42/ltc-tools/blob/master/ltcframeutil.c

/* what:
 *  bit 1: with user-fields/date
 *  bit 2: with parity
 */
int cmp_ltc_frametime(LTCFrame *a, LTCFrame *b, int what) {
  if ((what & 7) == 7)
    return memcmp(a, b, sizeof(LTCFrame));
  if (what & 4) {
    if (a->col_frame != b->col_frame ||
        a->binary_group_flag_bit1 != b->binary_group_flag_bit1 ||
        a->binary_group_flag_bit2 != b->binary_group_flag_bit2)
      return -1;
  }
  if (what & 2) {
    if (a->biphase_mark_phase_correction != b->biphase_mark_phase_correction)
      return -1;
  }
  if (what & 1) {
    if (a->user1 != b->user1 || a->user2 != b->user2 || a->user3 != b->user3 ||
        a->user4 != b->user4 || a->user5 != b->user5 || a->user6 != b->user6 ||
        a->user7 != b->user7 || a->user8 != b->user8)
      return -1;
  }
  if (a->frame_units != b->frame_units || a->frame_tens != b->frame_tens ||
      a->dfbit != b->dfbit || a->secs_units != b->secs_units ||
      a->secs_tens != b->secs_tens || a->mins_units != b->mins_units ||
      a->mins_tens != b->mins_tens || a->hours_units != b->hours_units ||
      a->hours_tens != b->hours_tens)
    return -1;

  return 0;
}

int detect_discontinuity(LTCFrameExt *frame, LTCFrameExt *prev, int fps,
                         int use_date, int fuzzyfps) {
  int discontinuity_detected = 0;

  if (fuzzyfps && ((frame->reverse && prev->ltc.frame_units == 0) ||
                   (!frame->reverse && frame->ltc.frame_units == 0))) {
    memcpy(prev, frame, sizeof(LTCFrameExt));
    return 0;
  }

  if (frame->reverse)
    ltc_frame_decrement(&prev->ltc, fps,
                        fps == 25 ? LTC_TV_625_50 : LTC_TV_525_60,
                        use_date ? LTC_USE_DATE : 0);
  else
    ltc_frame_increment(&prev->ltc, fps,
                        fps == 25 ? LTC_TV_625_50 : LTC_TV_525_60,
                        use_date ? LTC_USE_DATE : 0);
  if (cmp_ltc_frametime(&prev->ltc, &frame->ltc, use_date ? 1 : 0))
    discontinuity_detected = 1;
  memcpy(prev, frame, sizeof(LTCFrameExt));
  return discontinuity_detected;
}

//TODO: can't detect going from 30 to 25 only from 25 to 30
int detect_fps(int *fps, LTCFrameExt *frame, SMPTETimecode *stime) {
  int rv = 0;
  /* note: drop-frame-timecode fps rounded up, with the ltc.dfbit set */
  if (!fps)
    return -1;

  static int ff_cnt = 0;
  static int ff_max = 0;
  static LTCFrameExt prev;
  // TODO: return drop frame
  int df = (frame->ltc.dfbit) ? 1 : 0;

  if (!cmp_ltc_frametime(&prev.ltc, &frame->ltc, 0)) {
    ff_cnt = ff_max = 0;
  }
  if (detect_discontinuity(frame, &prev, *fps, 0, 1)) {
    ff_cnt = ff_max = 0;
    rv |= 0b00000100;
  }
  if (stime->frame > ff_max)
    ff_max = stime->frame;
  ff_cnt++;
  if (ff_cnt > 40 && ff_cnt > ff_max) {
    if (*fps != ff_max + 1) {
      // TODO: print here
      *fps = ff_max + 1;
      rv |= 0b00000001;
    }
    rv |= 0b00000010;
    ff_cnt = ff_max = 0;
  }
  return rv;
}

#define LTC_QUEUE_LENGTH 16

LTCDecoder *decoder = ltc_decoder_create(48000 / 30, LTC_QUEUE_LENGTH);
LTCFrameExt frame;
SMPTETimecode stime;
static int detected_fps = 0;

void LtcDecoderRead(const Nan::FunctionCallbackInfo<v8::Value> &info) {
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  if (info.Length() < 2 || info.Length() > 2) {
    Nan::ThrowTypeError("Wrong number of arguments");
    return;
  }
  if (!info[0]->IsArrayBufferView()) {
    Nan::ThrowTypeError("arguments 1 is not a buffer array");
    return;
  }
  if (!info[1]->IsNumber()) {
    Nan::ThrowTypeError("arguments 2 is not a number");
    return;
  }

  v8::Local<v8::ArrayBufferView> array =
      v8::Local<v8::ArrayBufferView>::Cast(info[0]);

  double offset = info[1]->NumberValue(context).FromJust();
  size_t len = array->ByteLength();
  uint8_t *buf = (uint8_t *)array->Buffer()->Data();

  ltc_decoder_write(decoder, buf, len, offset);
  // ltc_decoder_write_u16(decoder, buf, len, offset);
  int newFrame = ltc_decoder_read(decoder, &frame);
  ltc_frame_to_time(&stime, &frame.ltc, 0);

  if (newFrame) {
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    obj->Set(context, Nan::New("f").ToLocalChecked(), Nan::New(stime.frame));
    obj->Set(context, Nan::New("s").ToLocalChecked(), Nan::New(stime.secs));
    obj->Set(context, Nan::New("m").ToLocalChecked(), Nan::New(stime.mins));
    obj->Set(context, Nan::New("h").ToLocalChecked(), Nan::New(stime.hours));

    int notice = detect_fps(&detected_fps, &frame, &stime);
    obj->Set(context, Nan::New("fps").ToLocalChecked(), Nan::New(detected_fps));
    bool discontinuity_detected = notice & 0b00000100;
    obj->Set(context, Nan::New("discontinuity").ToLocalChecked(),
             Nan::New(discontinuity_detected));

    info.GetReturnValue().Set(obj);
  } else {
    v8::Local<v8::Primitive> n = Nan::Null();
    info.GetReturnValue().Set(n);
  }
}

void Init(v8::Local<v8::Object> exports) {
  v8::Local<v8::Context> context =
      exports->GetCreationContext().ToLocalChecked();
  exports->Set(context, Nan::New("LtcDecoderRead").ToLocalChecked(),
               Nan::New<v8::FunctionTemplate>(LtcDecoderRead)
                   ->GetFunction(context)
                   .ToLocalChecked());
}

NODE_MODULE(LtcDecoderRead, Init);