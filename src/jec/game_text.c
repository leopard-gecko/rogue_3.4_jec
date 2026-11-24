/* Game vocabulary: Rogue 3.4 entries rendered with Rogue 3.6 JEC wording.
 * English keys are the *original* 3.4 data and are deliberately not changed.
 * This module has no game-state side effects or random-number calls.
 */
#include <string.h>
#include "game_text.h"
#include "language.h"
struct translation { const char *en, *ja; };
static const struct translation terms[] = {
    {"giant ant", "巨大アリ"},
    {"bat", "こうもり"},
    {"centaur", "ケンタウロス"},
    {"dragon", "ドラゴン"},
    {"floating eye", "浮遊する目"},
    {"violet fungi", "菫の大キノコ"},
    {"gnome", "ノーム"},
    {"hobgoblin", "ホブゴブリン"},
    {"invisible stalker", "見えざる追手"},
    {"jackal", "ジャッカル"},
    {"kobold", "コボルド"},
    {"leprechaun", "レプラコーン"},
    {"mimic", "ミミック"},
    {"nymph", "ニンフ"},
    {"orc", "オーク"},
    {"purple worm", "紫の巨虫"},
    {"quasit", "小悪魔"},
    {"rust monster", "さびの怪物"},
    {"snake", "へび"},
    {"troll", "トロル"},
    {"umber hulk", "褐色の大あご虫"},
    {"vampire", "ヴァンパイア"},
    {"wraith", "死霊"},
    {"xorn", "ゾーン"},
    {"yeti", "イエティ"},
    {"zombie", "ゾンビ"},
    {"monster confusion", "怪物を混乱させる"},
    {"magic mapping", "魔法の地図の"},
    {"light", "灯りの"},
    {"hold monster", "怪物を封じこめる"},
    {"sleep", "眠りにおちる"},
    {"gain armor", "よろいに魔法をかける"},
    {"identify", "持ちものを識別する"},
    {"scare monster", "怪物を怖がらせる"},
    {"gold detection", "金塊を見つける"},
    {"teleportation", "テレポートする"},
    {"enchant a weapon", "武器に魔法をかける"},
    {"create a monster", "怪物を作りだす"},
    {"remove curse", "のろいを解く"},
    {"aggravate monsters", "怪物を怒らせる"},
    {"confusion", "頭が混乱する"},
    {"paralysis", "体がしびれる"},
    {"poison", "毒の"},
    {"gain strength", "強さが増す"},
    {"see invisible", "見えないものが見える"},
    {"healing", "傷を治す"},
    {"monster detection", "怪物探知の"},
    {"magic detection", "魔法探知の"},
    {"raise level", "レベルが上がる"},
    {"extra healing", "とてもよく傷を治す"},
    {"haste self", "素早くなる"},
    {"restore strength", "強さが元にもどる"},
    {"leather armor", "革のよろい"},
    {"ring mail", "鉄環のよろい"},
    {"studded leather armor", "鋲つき革のよろい"},
    {"scale mail", "鱗のよろい"},
    {"chain mail", "鎖のよろい"},
    {"splint mail", "鉄片のよろい"},
    {"banded mail", "帯金のよろい"},
    {"plate mail", "全身のよろい"},
    {"mace", "つちほこ"},
    {"long sword", "長い剣"},
    {"short bow", "短弓"},
    {"single arrow", "矢"},
    {"arrow", "矢"},
    {"dagger", "短剣"},
    {"rock", "石"},
    {"two handed sword", "両手持ちの剣"},
    {"sling", "投石器"},
    {"dart", "投げ矢"},
    {"crossbow", "クロスボウ"},
    {"crossbow bolt", "クロスボウの矢"},
    {"spear", "やり"},
    {"object", "品物"},
    {"wear", "よろいを身につける"},
    {"take off", "脱ぐ"},
    {"throw", "持ちものを投げる"},
    {"drop", "持ちものを落とす"},
    {"quaff", "水薬を飲む"},
    {"read", "巻き物を読む"},
    {"eat", "食べ物を食べる"},
    {"wield", "武器を装備する"},
    {"wall of a room", "部屋の壁"},
    {"gold", "金塊"},
    {"passage leading down", "隣の階への階段"},
    {"door", "部屋のドア"},
    {"room floor", "部屋の床"},
    {"you", "プレイヤー"},
    {"passage", "部屋と部屋をむすぶ通路"},
    {"trap", "隠されたわな"},
    {"potion", "水薬"},
    {"scroll", "巻き物"},
    {"food", "食糧"},
    {"weapon", "武器"},
    {"solid rock", "硬い岩"},
    {"armor", "よろい"},
    {"The Amulet of Yendor", "イェンダーの魔除け"},
    {"unknown character", "その文字は使われていない"},
    {"\tprints help", "\tヘルプを表示する"},
    {"\tidentify object", "\t画面の文字が表すものを表示"},
    {"\tleft", "\t左に進む"},
    {"\tdown", "\t下に進む"},
    {"\tup", "\t上に進む"},
    {"\tright", "\t右に進む"},
    {"\tup & left", "\t左上に進む"},
    {"\tup & right", "\t右上に進む"},
    {"\tdown & left", "\t左下に進む"},
    {"\tdown & right", "\t右下に進む"},
    {"\trun left", "\t左に走る"},
    {"\trun down", "\t下に走る"},
    {"\trun up", "\t上に走る"},
    {"\trun right", "\t右に走る"},
    {"\trun up & left", "\t左上に走る"},
    {"\trun up & right", "\t右上に走る"},
    {"\trun down & left", "\t左下に走る"},
    {"\trun down & right", "\t右下に走る"},
    {"\tthrow something", "\t物を投げる"},
    {"\tforward until find something", "\t何かを見つけるまで進む"},
    {"\tgo down a staircase", "\t階段を降りる"},
    {"\tsearch for trap/secret door", "\tわなと隠しとびらを探す"},
    {"\t(space) rest for a while", "\tしばらく休む"},
    {"\tinventory", "\t持ちものを見る"},
    {"\tinventory single item", "\t１つの持ちものを見る"},
    {"\tquaff potion", "\t水薬を飲む"},
    {"\tread paper", "\t巻き物を読む"},
    {"\teat food", "\t食べ物を食べる"},
    {"\twield a weapon", "\t武器を手に持つ"},
    {"\twear armor", "\tよろいを着る"},
    {"\ttake armor off", "\tよろいを脱ぐ"},
    {"\tdrop object", "\t持ちものを落とす"},
    {"\texamine/set options", "\tオプションの表示・設定"},
    {"\tredraw screen", "\t画面を描き直す"},
    {"\tcancel command", "\tコマンドを中止する"},
    {"\tprint program version number", "\tバージョン情報を表示する"},
    {"\trepeat last message", "\t最後のメッセージを再表示する"},
    {"\tshell escape", "\tシェルに抜ける"},
    {"\tsave game", "\tゲームをセーブして終了する"},
    {"\tquit", "\tゲームを終了する"},
    {"<dir>\tthrow something", "<方角>\t物を投げる"},
    {"<dir>\tforward until find something", "<方角>\t何かを見つけるまで進む"},
};
static const struct translation potion_colors[] = {
    {"red", "赤色"},
    {"blue", "青色"},
    {"green", "緑色"},
    {"yellow", "黄色"},
    {"black", "黒色"},
    {"brown", "茶色"},
    {"orange", "オレンジ色"},
    {"pink", "ピンク色"},
    {"purple", "紫色"},
    {"grey", "灰色"},
    {"white", "白色"},
    {"silver", "銀色"},
    {"gold", "金色"},
    {"violet", "すみれ色"},
    {"clear", "透明"},
    {"vermilion", "朱色"},
    {"ecru", "ベージュ色"},
    {"turquoise", "青緑色"},
    {"magenta", "赤紫色"},
    {"amber", "こはく色"},
    {"topaz", "淡褐色"},
    {"plaid", "格子柄"},
    {"tan", "淡い茶色"},
};
static const char *lookup(const struct translation *entries, size_t count,
                          const char *english)
{
    size_t i;
    if (!english) return "";
    if (!jrogue_lang_is_japanese) return english;
    for (i = 0; i < count; ++i)
        if (strcmp(english, entries[i].en) == 0)
            return entries[i].ja;
    return english;
}
const char *jrogue_text(const char *en)
{
    return lookup(terms, sizeof terms / sizeof terms[0], en);
}
const char *jrogue_color(const char *en)
{
    return lookup(potion_colors,
                  sizeof potion_colors / sizeof potion_colors[0], en);
}

/* The original 3.4 scroll titles are saved as English syllable strings.
 * Unlike 3.6, the 3.4 save format has no syllable-index records, so the
 * exact original syllable boundaries are unavailable on restoration.
 * Longest-prefix matching gives a predictable kana approximation without
 * consuming RNG or modifying save data; a few ambiguous titles may have
 * different kana boundaries from newly generated 3.6 titles.
 */
static const struct translation scroll_syllables[] = {
    {"a", "ア"},
    {"ab", "アブ"},
    {"ag", "アグ"},
    {"aks", "アクス"},
    {"ala", "アラ"},
    {"an", "アン"},
    {"ankh", "アンク"},
    {"app", "アプ"},
    {"arg", "アーグ"},
    {"arze", "アーゼ"},
    {"ash", "アッシュ"},
    {"ban", "バン"},
    {"bar", "バー"},
    {"bat", "バット"},
    {"bek", "ベク"},
    {"bie", "ビー"},
    {"bin", "ビン"},
    {"bit", "ビット"},
    {"bjor", "ビジョー"},
    {"blu", "ブル"},
    {"bot", "ボット"},
    {"bu", "ブ"},
    {"byt", "バイト"},
    {"comp", "コンプ"},
    {"con", "コン"},
    {"cos", "コス"},
    {"cre", "クレ"},
    {"dalf", "ダルフ"},
    {"dan", "ダン"},
    {"den", "デン"},
    {"do", "ド"},
    {"e", "エ"},
    {"eep", "イープ"},
    {"el", "エル"},
    {"eng", "エング"},
    {"er", "エー"},
    {"ere", "エレ"},
    {"erk", "イーク"},
    {"esh", "エッシュ"},
    {"evs", "エブス"},
    {"fa", "ファ"},
    {"fid", "フィッド"},
    {"for", "フォー"},
    {"fri", "フライ"},
    {"fu", "フー"},
    {"gan", "ガン"},
    {"gar", "ガー"},
    {"glen", "グレン"},
    {"gop", "ゴップ"},
    {"gre", "グレ"},
    {"ha", "ハー"},
    {"he", "ヘ"},
    {"hyd", "ヒド"},
    {"i", "イ"},
    {"ing", "イング"},
    {"ion", "イオン"},
    {"ip", "イップ"},
    {"ish", "イッシュ"},
    {"it", "イット"},
    {"ite", "アイト"},
    {"iv", "イブ"},
    {"jo", "ジョー"},
    {"kho", "コー"},
    {"kli", "クリ"},
    {"klis", "クリス"},
    {"la", "ラー"},
    {"lech", "レック"},
    {"man", "マン"},
    {"mar", "マー"},
    {"me", "メ"},
    {"mi", "マイ"},
    {"mic", "マイク"},
    {"mik", "ミク"},
    {"mon", "モン"},
    {"mung", "ムング"},
    {"mur", "マー"},
    {"nej", "ネジュ"},
    {"nelg", "ネルグ"},
    {"nep", "ネップ"},
    {"ner", "ナー"},
    {"nes", "ネス"},
    {"nes", "ネス"},
    {"nih", "ニー"},
    {"nin", "ニン"},
    {"o", "オ"},
    {"od", "オッド"},
    {"ood", "オード"},
    {"org", "オーグ"},
    {"orn", "オルン"},
    {"ox", "オックス"},
    {"oxy", "オキシー"},
    {"pay", "ペイ"},
    {"pet", "ペット"},
    {"ple", "プル"},
    {"plu", "プルー"},
    {"po", "ポ"},
    {"pot", "ポット"},
    {"prok", "プロック"},
    {"re", "リ"},
    {"rea", "レー"},
    {"rhov", "ロブ"},
    {"ri", "リー"},
    {"ro", "ロ"},
    {"rog", "ログ"},
    {"rok", "ロク"},
    {"rol", "ロル"},
    {"sa", "サ"},
    {"san", "サン"},
    {"sat", "サット"},
    {"see", "シー"},
    {"sef", "セフ"},
    {"seh", "ゼー"},
    {"shu", "シュ"},
    {"ski", "スキ"},
    {"sna", "スネイ"},
    {"sne", "スネ"},
    {"snik", "スニック"},
    {"sno", "スノ"},
    {"so", "ソ"},
    {"sol", "ソル"},
    {"sri", "スリ"},
    {"sta", "スタ"},
    {"sun", "サン"},
    {"ta", "タ"},
    {"tab", "タブ"},
    {"tem", "テム"},
    {"ther", "ザー"},
    {"ti", "ティ"},
    {"tox", "トックス"},
    {"trol", "トロール"},
    {"tue", "チュー"},
    {"turs", "ターズ"},
    {"u", "ウ"},
    {"ulk", "ウルク"},
    {"um", "ウム"},
    {"un", "ウン"},
    {"uni", "ウニ"},
    {"ur", "アー"},
    {"val", "ヴァル"},
    {"viv", "ヴィヴ"},
    {"vly", "ヴライ"},
    {"vom", "ヴォム"},
    {"wah", "ワー"},
    {"wed", "ウェド"},
    {"werg", "ワーグ"},
    {"wex", "ウェックス"},
    {"whon", "ホン"},
    {"wun", "ウン"},
    {"xo", "クソー"},
    {"y", "イ"},
    {"yot", "ヨット"},
    {"yu", "ユ"},
    {"zant", "ザント"},
    {"zap", "ザップ"},
    {"zeb", "ゼブ"},
    {"zim", "ジム"},
    {"zok", "ゾック"},
    {"zon", "ゾン"},
    {"zum", "ズム"},
};
/* The original English syllables are the source of the Japanese title.
 * init_names() checks the complete English AND Japanese length before storing
 * a title; display no longer needs a second, old-save-specific length cap.
 */
static size_t
convert_scroll_title_ja(const char *english, char *dst, size_t cap)
{
    const char *at = english;
    size_t used = 0, total = 0;

    if (dst != NULL && cap > 0)
        dst[0] = '\0';
    if (english == NULL)
        return 0;
    while (*at) {
        size_t best = 0, bytes, i;
        const char *kana = NULL;
        if (*at == ' ') {
            best = 1;
            kana = " ";
        } else {
            for (i = 0; i < sizeof scroll_syllables / sizeof scroll_syllables[0]; i++) {
                size_t length = strlen(scroll_syllables[i].en);
                if (length > best && strncmp(at, scroll_syllables[i].en, length) == 0) {
                    best = length;
                    kana = scroll_syllables[i].ja;
                }
            }
        }
        if (kana == NULL) {
            best = 1;
            kana = at;
            bytes = 1;
        } else
            bytes = strlen(kana);
        total += bytes;
        if (dst != NULL && cap > 0 && used + bytes < cap) {
            memcpy(dst + used, kana, bytes);
            used += bytes;
            dst[used] = '\0';
        }
        at += best;
    }
    return total;
}

size_t
jrogue_scroll_title_ja_bytes(const char *english)
{
    return convert_scroll_title_ja(english, NULL, 0);
}

const char *
jrogue_scroll_title(const char *english)
{
    static char japanese[JROGUE_SCROLL_TITLE_MAX_BYTES + 1];

    if (english == NULL)
        return "";
    if (!jrogue_lang_is_japanese)
        return english;
    convert_scroll_title_ja(english, japanese, sizeof japanese);
    return japanese;
}
