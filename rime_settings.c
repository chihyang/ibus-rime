#include "rime_config.h"
#include "rime_settings.h"
#include <errno.h>
#include <rime_api.h>
#include <string.h>

extern RimeApi *rime_api;

static struct ColorSchemeDefinition preset_color_schemes[] = {
  {
    .color_scheme_id = "aqua",
    .text_color = 0x606060,
    .back_color = 0xeeeeec,
    .candidate_text_color = 0x000000,
    .comment_text_color = 0x5a5a5a,
    .label_color = 0x000000,
    .hilited_text_color = 0x000000,
    .hilited_back_color = 0xd4d4d4,
    .hilited_candidate_text_color = 0xffffff,
    .hilited_candidate_back_color = 0x0a3afa,
    .hilited_comment_text_color = 0x9dacfc,
    .hilited_candidate_label_color = 0xffffff,
  },
  {
    .color_scheme_id = "azure",
    .text_color = 0x77a6cf,
    .back_color = 0x014e8b,
    .candidate_text_color = 0xcceaff,
    .comment_text_color = 0x6496c6,
    .label_color = 0xcceaff,
    .hilited_text_color = 0xcceaff,
    .hilited_back_color = 0x014e8b,
    .hilited_candidate_text_color = 0xffff7f,
    .hilited_candidate_back_color = 0x000000,
    .hilited_comment_text_color = 0x6496c6,
    .hilited_candidate_label_color = 0xffff7f,
  },
  {
    .color_scheme_id = "ink",
    .text_color = 0x5a5a5a,
    .back_color = 0xffffff,
    .candidate_text_color = 0x000000,
    .comment_text_color = 0x5a5a5a,
    .label_color = 0x000000,
    .hilited_text_color = 0x000000,
    .hilited_back_color = 0xdddddd,
    .hilited_candidate_text_color = 0xffffff,
    .hilited_candidate_back_color = 0x000000,
    .hilited_comment_text_color = 0x808080,
    .hilited_candidate_label_color = 0xffffff,
  },
  {
    .color_scheme_id = "luna",
    .text_color = 0xa5a5a5,
    .back_color = 0x000000,
    .candidate_text_color = 0xeeeeec,
    .comment_text_color = 0xa5a5a5,
    .label_color = 0xeeeeec,
    .hilited_text_color = 0xffff7f,
    .hilited_back_color = 0xffff7f,
    .hilited_candidate_text_color = 0xffff7f,
    .hilited_candidate_back_color = 0x000000,
    .hilited_comment_text_color = 0x9d9c44,
    .hilited_candidate_label_color = 0xffff7f,
  },
  { .color_scheme_id = NULL }
};

static struct ColorSchemeDefinition configured_color_scheme;

static struct IBusRimeSettings ibus_rime_settings_default = {
  .embed_preedit_text = TRUE,
  .preedit_style = PREEDIT_STYLE_COMPOSITION,
  .cursor_type = CURSOR_TYPE_INSERT,
  .lookup_table_orientation = IBUS_ORIENTATION_SYSTEM,
  .color_scheme = NULL,
};

struct IBusRimeSettings g_ibus_rime_settings;

static void init_custom_color_scheme(struct ColorSchemeDefinition* scheme)
{
  memset(scheme, 0, sizeof(*scheme));
  scheme->text_color = RIME_COLOR_UNSET;
  scheme->back_color = RIME_COLOR_UNSET;
  scheme->candidate_text_color = RIME_COLOR_UNSET;
  scheme->comment_text_color = RIME_COLOR_UNSET;
  scheme->label_color = RIME_COLOR_UNSET;
  scheme->hilited_text_color = RIME_COLOR_UNSET;
  scheme->hilited_back_color = RIME_COLOR_UNSET;
  scheme->hilited_candidate_text_color = RIME_COLOR_UNSET;
  scheme->hilited_candidate_back_color = RIME_COLOR_UNSET;
  scheme->hilited_comment_text_color = RIME_COLOR_UNSET;
  scheme->hilited_candidate_label_color = RIME_COLOR_UNSET;
}

static gboolean load_color(RimeConfig* config,
                           const char* color_scheme_id,
                           const char* field,
                           int* color)
{
  gchar* key = g_strdup_printf(
      "preset_color_schemes/%s/%s", color_scheme_id, field);
  const char* value = rime_api->config_get_cstring(config, key);
  if (!value) {
    g_free(key);
    return FALSE;
  }

  errno = 0;
  char* end = NULL;
  guint64 rime_color = g_ascii_strtoull(value, &end, 0);
  if (errno || end == value || *end || rime_color > G_MAXUINT32) {
    g_warning("color scheme '%s': invalid %s value '%s'",
              color_scheme_id, field, value);
    g_free(key);
    return FALSE;
  }

  const char* hex_digits = NULL;
  if (g_str_has_prefix(value, "0x") || g_str_has_prefix(value, "0X")) {
    hex_digits = value + 2;
  }
  const gboolean has_alpha =
      (hex_digits && strlen(hex_digits) > 6) || rime_color > 0xffffff;
  if (has_alpha) {
    const guint alpha = (rime_color >> 24) & 0xff;
    if (alpha == 0) {
      *color = RIME_COLOR_UNSET;
      g_free(key);
      return TRUE;
    }
    if (alpha != 0xff) {
      g_debug(
          "color scheme '%s': %s alpha channel is not supported by IBus",
          color_scheme_id, field);
    }
  }
  guint bgr = rime_color & 0xffffff;
  *color = ((bgr & 0x0000ff) << 16) |
           (bgr & 0x00ff00) |
           ((bgr & 0xff0000) >> 16);
  g_free(key);
  return TRUE;
}

static gboolean config_has_field(RimeConfig* config,
                                 const char* color_scheme_id,
                                 const char* field)
{
  gchar* key = g_strdup_printf(
      "preset_color_schemes/%s/%s", color_scheme_id, field);
  gboolean found = rime_api->config_get_cstring(config, key) != NULL;
  g_free(key);
  return found;
}

static void
select_color_scheme(struct IBusRimeSettings* settings,
                    RimeConfig* config,
                    const char* color_scheme_id)
{
  gboolean found_builtin = FALSE;
  struct ColorSchemeDefinition* c;
  for (c = preset_color_schemes; c->color_scheme_id; ++c) {
    if (!strcmp(c->color_scheme_id, color_scheme_id)) {
      configured_color_scheme = *c;
      found_builtin = TRUE;
      break;
    }
  }
  if (!found_builtin) {
    init_custom_color_scheme(&configured_color_scheme);
  }

  gboolean loaded = FALSE;
#define LOAD_COLOR(field) \
  loaded |= load_color(config, color_scheme_id, #field, \
                       &configured_color_scheme.field)
  LOAD_COLOR(text_color);
  LOAD_COLOR(back_color);
  LOAD_COLOR(candidate_text_color);
  LOAD_COLOR(comment_text_color);
  LOAD_COLOR(label_color);
  LOAD_COLOR(hilited_text_color);
  LOAD_COLOR(hilited_back_color);
  LOAD_COLOR(hilited_candidate_text_color);
  LOAD_COLOR(hilited_candidate_back_color);
  LOAD_COLOR(hilited_comment_text_color);
  LOAD_COLOR(hilited_candidate_label_color);
#undef LOAD_COLOR

  if (!found_builtin && !loaded) {
    g_warning("unknown or empty color scheme: %s", color_scheme_id);
    settings->color_scheme = NULL;
    return;
  }

  settings->color_scheme = &configured_color_scheme;
  g_debug("selected color scheme: %s", color_scheme_id);
  if (configured_color_scheme.back_color != RIME_COLOR_UNSET ||
      config_has_field(config, color_scheme_id, "border_color")) {
    g_warning(
        "color scheme '%s': window-level fields back_color and "
        "border_color are not supported by IBus; supported text colors "
        "will still be applied",
        color_scheme_id);
  }
}

void
ibus_rime_load_settings()
{
  g_ibus_rime_settings = ibus_rime_settings_default;

  RimeConfig config = {0};
  if (!rime_api->config_open("ibus_rime", &config)) {
    g_error("error loading settings for ibus_rime");
    return;
  }

  Bool inline_preedit = False;
  if (rime_api->config_get_bool(
          &config, "style/inline_preedit", &inline_preedit)) {
    g_ibus_rime_settings.embed_preedit_text = !!inline_preedit;
  }

  const char* preedit_style_str =
      rime_api->config_get_cstring(&config, "style/preedit_style");
  if(preedit_style_str) {
    if(!strcmp(preedit_style_str, "composition")) {
      g_ibus_rime_settings.preedit_style = PREEDIT_STYLE_COMPOSITION;
    } else if(!strcmp(preedit_style_str, "preview")) {
      g_ibus_rime_settings.preedit_style = PREEDIT_STYLE_PREVIEW;
    }
  }

  const char* cursor_type_str =
      rime_api->config_get_cstring(&config, "style/cursor_type");
  if (cursor_type_str) {
    if (!strcmp(cursor_type_str, "insert")) {
      g_ibus_rime_settings.cursor_type = CURSOR_TYPE_INSERT;
    } else if (!strcmp(cursor_type_str, "select")) {
      g_ibus_rime_settings.cursor_type = CURSOR_TYPE_SELECT;
    }
  }

  Bool horizontal = False;
  if (rime_api->config_get_bool(&config, "style/horizontal", &horizontal)) {
    g_ibus_rime_settings.lookup_table_orientation =
      horizontal ? IBUS_ORIENTATION_HORIZONTAL : IBUS_ORIENTATION_VERTICAL;
  }

  const char* color_scheme =
    rime_api->config_get_cstring(&config, "style/color_scheme");
  if (color_scheme) {
    select_color_scheme(&g_ibus_rime_settings, &config, color_scheme);
  }

  rime_api->config_close(&config);
}
