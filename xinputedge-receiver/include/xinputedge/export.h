#ifndef XINPUTEDGE_EXPORT_H
#define XINPUTEDGE_EXPORT_H

/*
 * XINPUTEDGE_API — 公開 API 関数に付けるエクスポートマクロ
 *
 * 現在は static library のため空マクロだが、将来 shared library
 * (.dll / .so) を作る場合でも ABI を壊さずに対応できる。
 *
 * ビルド時に -DXINPUTEDGE_EXPORT を定義すると dllexport モードになる。
 */
#ifdef _WIN32
#ifdef XINPUTEDGE_EXPORT
#define XINPUTEDGE_API __declspec(dllexport)
#else
#define XINPUTEDGE_API __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define XINPUTEDGE_API __attribute__((visibility("default")))
#else
#define XINPUTEDGE_API
#endif
#endif

/* static library としてリンクする場合はこちらを使う */
#ifdef XINPUTEDGE_STATIC
#undef XINPUTEDGE_API
#define XINPUTEDGE_API
#endif

#endif /* XINPUTEDGE_EXPORT_H */
