/*
 * cli_robot_sim — XInputEdge CLI ロボットシミュレータ
 *
 * Windows PC side:
 *   XInputEdge sender → UDP → (this program on Raspberry Pi / Linux)
 *
 * 操作方法:
 *   左スティック  : ロボット移動
 *   右スティック X: 旋回 (yaw)
 *   LT / RT       : ブースト / ブレーキ表示
 *   Aボタン        : アクション (★)
 *   STARTボタン    : 終了
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include <xinputedge/xinputedge.h>

/* ------------------------------------------------------------------ */
/* 定数                                                                 */
/* ------------------------------------------------------------------ */
#define LISTEN_PORT 5000
#define FRAME_US 33333 /* ~30 Hz */

#define MAP_W 50
#define MAP_H 20

/* スティック感度スケール (32767 を 1.0 に正規化) */
#define STICK_SCALE 32767.0f
/* 1 フレームあたりの最大移動量 (セル単位) */
#define MOVE_SPEED 0.15f
/* 1 フレームあたりの最大旋回量 (度) */
#define YAW_SPEED 3.0f

/* ------------------------------------------------------------------ */
/* グローバル状態                                                        */
/* ------------------------------------------------------------------ */
static XieServer *g_server = NULL;
static volatile int g_running = 1;

/* ロボット状態 */
typedef struct {
  float x;
  float y;
  float yaw; /* 度: 0=上, 90=右, 180=下, 270=左 */
  int action; /* Aボタンフラッシュカウンタ */
} Robot;

/* ------------------------------------------------------------------ */
/* シグナルハンドラ                                                      */
/* ------------------------------------------------------------------ */
static void sig_handler(int s) {
  (void)s;
  g_running = 0;
}

/* ------------------------------------------------------------------ */
/* ネットワークスレッド                                                  */
/* ------------------------------------------------------------------ */
static void *network_thread(void *arg) {
  XieServer *server = (XieServer *)arg;
  while (g_running) {
    xie_server_recv(server); /* 内部でタイムアウト付きブロック */
  }
  return NULL;
}

/* ------------------------------------------------------------------ */
/* ANSI ユーティリティ                                                   */
/* ------------------------------------------------------------------ */
static void ansi_clear(void) {
  printf("\033[2J\033[H");
}

static void ansi_move(int row, int col) {
  printf("\033[%d;%dH", row, col);
}

static void ansi_color(int code) {
  printf("\033[%dm", code);
}

static void ansi_reset(void) {
  printf("\033[0m");
}

/* ------------------------------------------------------------------ */
/* 方向からロボット文字を決定                                             */
/* ------------------------------------------------------------------ */
static const char *robot_char(float yaw) {
  /* 45 度ごとに 8 方向 */
  int d = (int)((yaw + 22.5f) / 45.0f) % 8;
  static const char *dirs[] = {"↑", "↗", "→", "↘", "↓", "↙", "←", "↖"};
  return dirs[d];
}

/* ------------------------------------------------------------------ */
/* スティック値からバー描画                                               */
/* ------------------------------------------------------------------ */
static void draw_stick_bar(const char *label, float val, int row, int col) {
  ansi_move(row, col);
  printf("%s [", label);
  int center = 10;
  for (int i = 0; i < 21; i++) {
    if (i == center) {
      printf("|");
    } else if (val > 0 && i > center && i <= center + (int)(val * 10)) {
      ansi_color(32); /* 緑 */
      printf("█");
      ansi_reset();
    } else if (val < 0 && i < center && i >= center + (int)(val * 10)) {
      ansi_color(31); /* 赤 */
      printf("█");
      ansi_reset();
    } else {
      printf("·");
    }
  }
  printf("] %+.2f", val);
}

/* ------------------------------------------------------------------ */
/* 画面描画                                                              */
/* ------------------------------------------------------------------ */
static void draw(const Robot *robot, const XieState *state, int timeout,
                 uint32_t lost) {
  int rx = (int)robot->x;
  int ry = (int)robot->y;

  /* クランプ */
  if (rx < 1) rx = 1;
  if (rx > MAP_W) rx = MAP_W;
  if (ry < 1) ry = 1;
  if (ry > MAP_H) ry = MAP_H;

  ansi_clear();

  /* ── タイトル ── */
  ansi_color(36);
  printf("  XInputEdge CLI Robot Simulator\n");
  ansi_reset();
  printf("  Port :%d  |  LOST:%u  |  ", LISTEN_PORT, lost);

  if (timeout) {
    ansi_color(31);
    printf("⚠ TIMEOUT / SAFE STATE");
    ansi_reset();
  } else {
    ansi_color(32);
    printf("● CONNECTED");
    ansi_reset();
  }
  printf("\n");

  /* ── 外枠（MAP_W+2 文字幅, MAP_H+2 行高）── */
  printf("  +");
  for (int x = 0; x < MAP_W; x++) printf("-");
  printf("+\n");

  for (int y = 1; y <= MAP_H; y++) {
    printf("  |");
    for (int x = 1; x <= MAP_W; x++) {
      if (x == rx && y == ry) {
        if (robot->action > 0) {
          ansi_color(33); /* 黄 */
          printf("★");
          ansi_reset();
        } else {
          ansi_color(36); /* シアン */
          printf("%s", robot_char(robot->yaw));
          ansi_reset();
        }
      } else {
        printf(" ");
      }
    }
    printf("|\n");
  }

  printf("  +");
  for (int x = 0; x < MAP_W; x++) printf("-");
  printf("+\n");

  /* ── 座標・向き ── */
  printf("\n");
  printf("  pos: (%2d, %2d)  yaw: %3.0f deg\n", rx, ry, robot->yaw);

  /* ── スティックバー ── */
  float lx = (float)state->lx / STICK_SCALE;
  float ly = (float)state->ly / STICK_SCALE;
  float rx_v = (float)state->rx / STICK_SCALE;

  int base_row = MAP_H + 7;
  draw_stick_bar("LX", lx, base_row, 1);
  draw_stick_bar("LY", ly, base_row + 1, 1);
  draw_stick_bar("RX", rx_v, base_row + 2, 1);

  /* ── トリガー ── */
  ansi_move(base_row + 4, 1);
  printf("LT: %3d  RT: %3d", state->lt, state->rt);

  /* ── ボタン表示 ── */
  ansi_move(base_row + 5, 1);
  printf("BTN: ");
  struct {
    uint16_t mask;
    const char *name;
    int color;
  } btns[] = {
      {XIE_BTN_A, "A", 32},     {XIE_BTN_B, "B", 31},
      {XIE_BTN_X, "X", 34},     {XIE_BTN_Y, "Y", 33},
      {XIE_BTN_LB, "LB", 35},   {XIE_BTN_RB, "RB", 35},
      {XIE_BTN_START, "ST", 37}, {0, NULL, 0},
  };
  for (int i = 0; btns[i].name; i++) {
    if (state->buttons & btns[i].mask) {
      ansi_color(btns[i].color);
      printf("[%s] ", btns[i].name);
      ansi_reset();
    } else {
      printf("[  ] ");
    }
  }

  /* ── 操作ガイド ── */
  ansi_move(base_row + 7, 1);
  ansi_color(90);
  printf("左スティック:移動  右スティックX:旋回  Aボタン:アクション  START:終了");
  ansi_reset();

  fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  /* カーソル非表示 */
  printf("\033[?25l");
  fflush(stdout);

  g_server = xie_server_create();
  if (!g_server) {
    fprintf(stderr, "xie_server_create() failed\n");
    return 1;
  }

  if (xie_server_init(g_server, "0.0.0.0", LISTEN_PORT) != XIE_OK) {
    fprintf(stderr, "xie_server_init() failed (port %d)\n", LISTEN_PORT);
    xie_server_destroy(g_server);
    return 1;
  }

  pthread_t net_th;
  pthread_create(&net_th, NULL, network_thread, g_server);

  /* ロボット初期位置: マップ中央 */
  Robot robot = {
      .x = MAP_W / 2.0f,
      .y = MAP_H / 2.0f,
      .yaw = 0.0f,
      .action = 0,
  };

  while (g_running) {
    int timeout = xie_server_is_timeout(g_server);
    uint32_t lost = xie_server_lost(g_server);
    const XieState *state = xie_server_state(g_server);

    if (!timeout && state) {
      float lx = (float)state->lx / STICK_SCALE;
      float ly = (float)state->ly / STICK_SCALE;
      float rx = (float)state->rx / STICK_SCALE;

      /* デッドゾーン 10% */
      if (lx > -0.1f && lx < 0.1f) lx = 0.0f;
      if (ly > -0.1f && ly < 0.1f) ly = 0.0f;
      if (rx > -0.1f && rx < 0.1f) rx = 0.0f;

      robot.x += lx * MOVE_SPEED;
      robot.y += ly * MOVE_SPEED;
      robot.yaw += rx * YAW_SPEED;
      if (robot.yaw < 0.0f) robot.yaw += 360.0f;
      if (robot.yaw >= 360.0f) robot.yaw -= 360.0f;

      /* 境界クランプ */
      if (robot.x < 1.0f) robot.x = 1.0f;
      if (robot.x > (float)MAP_W) robot.x = (float)MAP_W;
      if (robot.y < 1.0f) robot.y = 1.0f;
      if (robot.y > (float)MAP_H) robot.y = (float)MAP_H;

      /* Aボタン: アクションフラッシュ */
      if (state->buttons & XIE_BTN_A) {
        robot.action = 5;
      } else if (robot.action > 0) {
        robot.action--;
      }

      /* STARTボタン: 終了 */
      if (state->buttons & XIE_BTN_START) {
        g_running = 0;
      }
    }

    /* 描画 (state が null のときはダミー状態を渡す) */
    static XieState empty_state = {0};
    draw(&robot, (state && !timeout) ? state : &empty_state, timeout, lost);

    xie_sleep_us(FRAME_US);
  }

  /* クリーンアップ */
  pthread_join(net_th, NULL);
  xie_server_close(g_server);
  xie_server_destroy(g_server);

  /* カーソルと画面を元に戻す */
  printf("\033[?25h\033[2J\033[H");
  printf("Bye! XInputEdge CLI Robot Simulator\n");

  return 0;
}
