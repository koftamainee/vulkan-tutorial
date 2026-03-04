#pragma once

#include <string>

enum class EResult : uint8_t {
  Success,
  SwapChainOutOfDate,
  SwapChainSuboptimal,
  FailedToSubmitToQueue,
};

constexpr std::string_view ToString(EResult Result) {
  switch (Result) {
    // clang-format off
  case EResult::Success:              return "Success";
  case EResult::SwapChainOutOfDate:   return "SwapChainOutOfDate";
  case EResult::SwapChainSuboptimal:  return "SwapChainSuboptimal";
  case EResult::FailedToSubmitToQueue: return "FailedToSubmitToQueue";
  default:                            return "Unknown";
    // clang-format on
  }
}

#ifdef NDEBUG
#define DO_CHECK 0
#else
#define DO_CHECK 1
#endif

[[noreturn]] inline void FDebug_AssertFailed(const char *Expr,
                                             const char *File,
                                             int32_t Line) {
  std::fprintf(stderr, "[ASSERT] %s\n  File: %s\n  Line: %d\n", Expr, File, Line);
  std::abort();
}

[[noreturn]] inline void FDebug_FatalFailed(const char *Msg,
                                            const char *File,
                                            int32_t Line) {
  std::fprintf(stderr, "[FATAL] %s\n  File: %s\n  Line: %d\n", Msg, File, Line);
  std::abort();
}

inline void FDebug_EnsureFailed(const char *Expr,
                                const char *File,
                                int32_t Line) {
  std::fprintf(stderr, "[ENSURE] %s\n  File: %s\n  Line: %d\n", Expr, File, Line);
}


#if DO_CHECK
#define check(Expr) \
        do { \
            if (!(Expr)) { \
                FDebug_AssertFailed(#Expr, __FILE__, __LINE__); \
            } \
        } while(0)
#else
#define check(Expr)
#endif

#if DO_CHECK
#define checkf(Expr, Msg) \
        do { \
            if (!(Expr)) { \
                FDebug_AssertFailed(Msg, __FILE__, __LINE__); \
            } \
        } while(0)
#else
#define checkf(Expr, Msg)
#endif

#define fatal(Expr, Msg) \
    do { \
        if (Expr) { \
            FDebug_FatalFailed(Msg, __FILE__, __LINE__); \
        } \
    } while(0)

#if DO_CHECK
#define verify(Expr) \
        [&]() -> bool { \
            if (!(Expr)) { \
                FDebug_AssertFailed(#Expr, __FILE__, __LINE__); \
                return false; \
            } \
            return true; \
        }()
#else
#define verify(Expr) (!!(Expr))
#endif


#if DO_CHECK
#define ensure(Expr) \
        [&]() -> bool { \
            if (!(Expr)) { \
                FDebug_EnsureFailed(#Expr, __FILE__, __LINE__); \
                return false; \
            } \
            return true; \
        }()
#else
#define ensure(Expr) (!!(Expr))
#endif


#define vk_check(VkCall) \
    do { \
        const VkResult _VkResult = (VkCall); \
        checkf(_VkResult == VK_SUCCESS, "VkResult != VK_SUCCESS"); \
    } while(0)
