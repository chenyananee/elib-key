/* elib_key_err.h - Key Detection Error Codes */
#ifndef ELIB_KEY_ERR_H
#define ELIB_KEY_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ELIB_KEY_OK = 0,
    ELIB_KEY_ERR_INVALID_PARAM,
    ELIB_KEY_ERR_NOT_INITIALIZED,
    ELIB_KEY_ERR_ALREADY_INITIALIZED,
    ELIB_KEY_ERR_TOO_MANY_KEYS,
    ELIB_KEY_ERR_TOO_MANY_COMBOS,
} elib_key_err_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_KEY_ERR_H */
