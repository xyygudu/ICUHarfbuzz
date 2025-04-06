// ICU_test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <unicode/brkiter.h>
#include <unicode/utext.h>
#include <string>
#include <vector>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <unicode/ubidi.h>
#include <unicode/utypes.h>
#include <unicode/uscript.h>
#include <unicode/utext.h>

#define WIDTH   400
#define HEIGHT  80


/* origin is the upper left corner */
unsigned char image[HEIGHT][WIDTH];

// 定义文本段结构
struct TextSegment {
    int32_t start;      // 起始位置
    int32_t end;        // 结束位置
    UScriptCode script; // 脚本类型
};

// 输入：UTF-16 字符串和长度
std::vector<TextSegment> get_scripts(const UChar* text, int32_t logicStart, int32_t runLength) {
    std::vector<TextSegment> segments;
    int32_t current_start = 0;
    UScriptCode current_script = USCRIPT_INVALID_CODE;
    const UChar* runText = text + logicStart;
    for (int32_t i = 0; i < runLength; ) {
        UChar32 c;
        U16_GET(runText, 0, i, runLength, c); // 处理 UTF-16 代理对，通常返回Unicode码点

        
        UErrorCode error = U_ZERO_ERROR;
        UScriptCode script = uscript_getScript(c, &error);

        if (U_FAILURE(error)) {
            script = USCRIPT_UNKNOWN;
        }

        // 如果脚本变化或首次循环，创建新段
        if (script != current_script || i == 0) {
            if (current_script != USCRIPT_INVALID_CODE) {
                segments.push_back({ current_start + logicStart, i + logicStart, current_script });
            }
            current_start = i;
            current_script = script;
        }

        // 移动到下一个字符（考虑代理对）
        i += U16_LENGTH(c);
    }

    // 添加最后一个段
    if (current_start < runLength) {
        segments.push_back({ current_start + logicStart, runLength + logicStart, current_script });
    }

    // 打印分段结果
    for (const auto& seg : segments) {
        std::cout << "Segment [" << seg.start << "-" << seg.end << "]: Script=" << uscript_getName(seg.script) << std::endl;
    }

    return segments;
}

void bidi_process(const UChar* text, int32_t length)
{
    UErrorCode status;
    UBiDi* bidi = ubidi_open();
    ubidi_setPara(bidi, text, length, UBIDI_DEFAULT_LTR, nullptr, &status);

    int32_t visualToLogical[100] = { 0 };
    ubidi_getVisualMap(bidi, visualToLogical, &status);

    // 输出视觉顺序的文本
    std::cout << "idx:char" << std::endl;
    std::cout << "Visual order: ";
    for (int32_t i = 0; i < length; i++) {
        int32_t logicalIndex = visualToLogical[i];
        UChar c = text[logicalIndex];
        std::cout << logicalIndex << ":" << c << " ";
    }
    std::cout << std::endl;

    ubidi_close(bidi);
}


hb_script_t uscript_to_hb_script(UScriptCode script_code) {
    const char* short_name = uscript_getShortName(script_code);
    return hb_script_from_string(short_name, 4);
}

hb_direction_t ubidi_to_hb_direction(UBiDiDirection bidi_dir, UBiDiDirection para_dir = UBIDI_LTR) {
    switch (bidi_dir) {
    case UBIDI_LTR:
        return HB_DIRECTION_LTR;
    case UBIDI_RTL:
        return HB_DIRECTION_RTL;
    default:
        return para_dir == UBIDI_RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR;
    }
}

std::string get_font_file(hb_script_t script = HB_SCRIPT_UNKNOWN)
{
    return u8"E:/WorkSpace/Fonts/Arial Unicode MS.ttf";
}

void get_glyphs(const UChar* text, int32_t length, std::vector<hb_glyph_info_t>& all_glyphs, std::vector<hb_glyph_position_t>& all_positions)
{
    UErrorCode error;
    UBiDi* bidi = ubidi_open();
    
    ubidi_setPara(bidi, text, length, UBIDI_DEFAULT_LTR, nullptr, &error);
    int32_t runCount = ubidi_countRuns(bidi, &error);
    std::cout << "bidi count runs: " << runCount << std::endl;
    
    for (size_t runIdx = 0; runIdx < runCount; runIdx++)
    {
        int32_t logicalStart, runLength;
        UBiDiDirection runDir = ubidi_getVisualRun(bidi, runIdx, &logicalStart, &runLength);

        std::vector<TextSegment> scriptsOfRun = get_scripts(text, logicalStart, runLength);

        for(auto& seg : scriptsOfRun)
        {
            int32_t start = seg.start;
            int32_t end = seg.end;
            const uint16_t* hbText = reinterpret_cast<const uint16_t*>(text + start);
            hb_buffer_t* buffer = hb_buffer_create();
            hb_buffer_add_utf16(buffer, hbText, end - start, 0, -1);
            hb_buffer_set_direction(buffer, ubidi_to_hb_direction(runDir));
            hb_script_t script = uscript_to_hb_script(seg.script);
            hb_buffer_set_script(buffer, script);
            const char* default_lang = (script == HB_SCRIPT_ARABIC) ? "ar" :
                (script == HB_SCRIPT_HAN) ? "zh" : "en";
            hb_buffer_set_language(buffer, hb_language_from_string(default_lang, -1));

            hb_blob_t* blob = hb_blob_create_from_file(get_font_file(script).c_str()); 
            hb_face_t* face = hb_face_create(blob, 0);
            hb_font_t* font = hb_font_create(face);

            hb_shape(font, buffer, nullptr, 0);

            unsigned int glyph_count;
            hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
            hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buffer, &glyph_count);
            
            std::cout << "codepoint" << "\t" << "cluster" << "\t\t" << "x_advance" << std::endl;
            for (unsigned int i = 0; i < glyph_count; i++) {
                glyph_info[i].cluster += start;
                all_glyphs.push_back(glyph_info[i]);
                all_positions.push_back(glyph_pos[i]);
                hb_codepoint_t glyphid = glyph_info[i].codepoint;  // 这里的codepoint表示glyphid，并不是字符的unicode码点
                uint32_t cluster = glyph_info[i].cluster;
                hb_position_t x_offset = glyph_pos[i].x_offset;
                hb_position_t y_offset = glyph_pos[i].y_offset;
                hb_position_t x_advance = glyph_pos[i].x_advance;
                hb_position_t y_advance = glyph_pos[i].y_advance;
                std::cout << glyphid << "\t\t" << cluster << "\t\t" << x_advance << std::endl;
            }

            hb_buffer_destroy(buffer);
            hb_font_destroy(font);
            hb_face_destroy(face);
            hb_blob_destroy(blob);
        }
       
    }

}

void show_image(void)
{
    int  i, j;
    for (i = 0; i < HEIGHT; i++)
    {
        for (j = 0; j < WIDTH; j++)
            putchar(image[i][j] == 0 ? ' '
                : image[i][j] < 128 ? '+'
                : '*');
        putchar('\n');
    }
}

void draw_bitmap(FT_Bitmap* bitmap, FT_Int x, FT_Int y)
{
    FT_Int  i, j, p, q;
    FT_Int  x_max = x + bitmap->width;
    FT_Int  y_max = y + bitmap->rows;


    /* for simplicity, we assume that `bitmap->pixel_mode' */
    /* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */

    for (i = x, p = 0; i < x_max; i++, p++)
    {
        for (j = y, q = 0; j < y_max; j++, q++)
        {
            if (i < 0 || j < 0 ||
                i >= WIDTH || j >= HEIGHT)
                continue;

            image[j][i] |= bitmap->buffer[q * bitmap->width + p];
        }
    }
}

int main()
{
    // std::string utf8_text = u8"Helloالعالم🥳你好";
    std::string utf8_text = u8"الْعَرَبِيَّةُ";

    hb_buffer_t* buf;
    buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, utf8_text.c_str(), -1, 0, -1);

    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(utf8_text.c_str()));

    const UChar* text = ustr.getBuffer();
    int32_t length = ustr.length();
    //get_scripts(text, length);

    bidi_process(text, length);

    std::vector<hb_glyph_info_t> all_glyphs;
    std::vector<hb_glyph_position_t> all_positions;
    get_glyphs(text, length, all_glyphs, all_positions);


    FT_Library    library;
    FT_Face       face;

    FT_GlyphSlot  slot;
    FT_Matrix     matrix;                 /* transformation matrix */
    FT_Vector     pen;                    /* untransformed origin  */
    FT_Error      error;

    double        angle;
    int           target_height;


    std::string fontFile = get_font_file(); 
    const char* filename = fontFile.c_str();
    angle = /*(25.0 / 360) * 3.14159 * 2*/0;      /* use 25 degrees     */
    target_height = HEIGHT * 3 / 4;

    error = FT_Init_FreeType(&library);              /* initialize library */
    /* error handling omitted */

    error = FT_New_Face(library, filename, 0, &face);/* create face object */
    /* error handling omitted */

    /* use 50pt at 100dpi */
    error = FT_Set_Char_Size(face, 20 * 64, 0, 100, 0);                /* set character size */
    slot = face->glyph;

    /* set up matrix */
    matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

    pen.x = 10;
    pen.y = target_height;

    for (int i = 0; i < all_glyphs.size(); i++)
    {
        //error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
        error = FT_Load_Glyph(face, all_glyphs[i].codepoint, FT_LOAD_RENDER);
        if (error)
            continue;                 /* ignore errors */

        /* now, draw to our target surface (convert position) */
        draw_bitmap(&slot->bitmap, pen.x + slot->bitmap_left, pen.y - slot->bitmap_top);

        /* increment pen position */
        //pen.x += slot->advance.x >> 6;
        //pen.y += slot->advance.y;
        pen.x += all_positions[i].x_advance >> 6;
        pen.y += all_positions[i].y_advance >> 6;
    }

    show_image();

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    




    // 创建行断点迭代器（Line Break Iterator）
    icu::BreakIterator* bi = icu::BreakIterator::createCharacterInstance(
        icu::Locale::getDefault(), status
    );

    if (U_FAILURE(status)) {
        // 错误处理
    }

    // 设置文本到迭代器
    UText* ut = utext_openUnicodeString(nullptr, &ustr, &status);
    bi->setText(ut, status);

    // 遍历所有断点
    int32_t start = bi->first();
    std::vector<int32_t> break_positions;
    for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {
        break_positions.push_back(end);
    }
    float max_line_width = 200.0f; // 文本框宽度
    float current_line_width = 0.0f;
    std::vector<std::string> lines;

    start = 0;
    // 遍历断点并计算宽度
    for (size_t i = 0; i < break_positions.size(); ++i) {
        int32_t end_pos = break_positions[i];
        icu::UnicodeString line_substr;
        ustr.extractBetween(start, end_pos, line_substr);

        // 将 UnicodeString 转为 UTF-8
        std::string line_utf8;
        line_substr.toUTF8String(line_utf8);
        start = end_pos;
        // 计算子字符串宽度（需结合 Harfbuzz 和 FreeType）
        //hb_buffer_clear_contents(hb_buffer);
        //hb_buffer_add_utf8(hb_buffer, line_utf8.c_str(), -1, 0, -1);
        //hb_shape(hb_font, hb_buffer, nullptr, 0);

        // 计算总宽度
        /*unsigned int glyph_count;
        hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buffer, &glyph_count);
        hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(hb_buffer, &glyph_count);*/

        //float line_width = 0.0f;
        //for (unsigned int j = 0; j < glyph_count; ++j) {
        //    line_width += glyph_pos[j].x_advance / 64.0f; // 转换为像素
        //}

        //// 判断是否超过最大宽度
        //if (current_line_width + line_width > max_line_width) {
        //    // 换行并重置当前行宽度
        //    lines.push_back(current_line);
        //    current_line.clear();
        //    current_line_width = 0.0f;
        //}

        //current_line += line_utf8;
        //current_line_width += line_width;
    }
}

