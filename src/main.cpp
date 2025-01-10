#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#ifndef LINUX_BUILD
#include "nvs_flash.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include <M5Unified.h>
#ifdef CONFIG_ENABLE_AVATAR
#include <Avatar.h>
#endif

constexpr const char* TAG = "main";

#ifdef CONFIG_ENABLE_HEAP_MONITOR
static esp_timer_handle_t s_monitor_timer;
#endif // CONFIG_ENABLE_HEAP_MONITOR

#ifdef CONFIG_ENABLE_AVATAR
//M5Canvas canvas = M5Canvas(&M5.Lcd);    //M5GFX test
using namespace m5avatar;
Avatar avatar;
const Expression expressions_table[] = {
  Expression::Neutral,
  Expression::Happy,
  Expression::Sleepy,
  Expression::Doubt,
  Expression::Sad,
  Expression::Angry
};
#endif

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

#ifdef CONFIG_ENABLE_HEAP_MONITOR
  esp_timer_create_args_t timer_args = {
      .callback = [](void* arg) {
    ESP_LOGW(TAG, "current heap %7d | minimum ever %7d | largest free %7d ",
             xPortGetFreeHeapSize(),
             xPortGetMinimumEverFreeHeapSize(),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
      },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "monitor_timer"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_monitor_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(s_monitor_timer, CONFIG_HEAP_MONITOR_INTERVAL_MS * 1000ULL));
#endif // CONFIG_ENABLE_HEAP_MONITOR

  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);

#ifdef CONFIG_ENABLE_AVATAR
#if 0 //M5GFX test
  canvas.setColorDepth(8);  // カラーモード設定
  canvas.createSprite(M5.Display.width(), M5.Display.height()); // canvasサイズ（メモリ描画領域）設定（画面サイズに設定）
  canvas.setTextSize(2);
  canvas.fillScreen(BLACK);
  canvas.setCursor(0, 20);               // Move the cursor position to (x,y).
  canvas.setTextColor(BLUE, BLACK);
  canvas.print("LCD test LCD test ");
  canvas.pushSprite(0, 0);  // メモリ内に描画したcanvasを座標を指定して表示する
#endif

  avatar.init();
  //avatar.addTask(lipSync, "lipSync");
  //avatar.addTask(servo, "servo");
  //avatar.setSpeechFont(&fonts::efontJA_16);   //これを有効にすると、現状のパーティション設定だとプログラム領域オーバー
#endif  //CONFIG_ENABLE_AVATAR

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_wifi();
  
  oai_init_audio_capture();
  oai_init_audio_decoder();
  
  oai_webrtc();
}
#else
int main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_webrtc();
}
#endif
