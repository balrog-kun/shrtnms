/*
 * Written by: Andrzej Zaborowski <andrew.zaborowski@intel.com>
 *
 * Code in this file is for now licensed under the 2-clause BSD license.
 */

/*
 * Functions like strcasecmp and strcasecmp_l seem to ignore the locale
 * and non-ascii characters on Linux (glibc?), but not on darwin (bsd stuff?).
 * However they work fine with wide char (wchar_t) in both forms:
 * wcscasecmp and wcscasecmp_l.  So to support Linux we end up using the
 * wide char versions and converting all strings back and forth. (for now)
 *
 * Unfortunately it seems functions like mbrlen don't even have a _l
 * counterpart and can only work with the per-thread locales.  And they're
 * not fixed at UTF-8, mbrlen() only returns -1 when locale is unset.  We
 * avoid them for now.
 *
 * If anyone knows the proper way to do UTF-8 in C, please fix this.  But,
 * it didn't seem like either ICU or Glib bring any substantial improvement
 * in API complexity.
 */

#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <xlocale.h>

/*
 * Can't find wcsrtombs_l or mbsrtowcs_l on my Linux install :(  Works on
 * Darwin again.
 */
#ifdef linux
# define mbsrtowcs_l(a, b, c, d, l) mbsrtowcs(a, b, c, d)
# define wcsrtombs_l(a, b, c, d, l) wcsrtombs(a, b, c, d)
#endif

#include "shortnames.h"

/*
 * Words or phrases together with their abbreviations.  It is assumed that
 * these phrases are common and in extreme cases can be omitted altogether
 * from the map display.  Given a full name of something we'll produce two
 * new strings: the abbreviated name, and the "stem" which skips all the
 * more common words and phrases.
 *
 * Note that the input is always assumed to be unabbreviated.  If there are
 * already abbreviations in the input, the "stem" output will include them,
 * while the idea is that it shouldn't.
 */
static const wchar_t *abbrevs[] = {
    /* Polish */
    L"plac", L"pl.",
    L"ulica", L"ul.",
    L"aleja", L"al.",
    L"generała", L"gen.",
    L"księdza", L"ks.",
    L"księży", L"ks.",
    L"księcia", L"ks.",
    L"książąt", L"ks.",
    L"biskupa", L"bp",
    L"arcybiskupa", L"abp",
    L"doktora", L"dr",
    L"inżyniera", L"inż.",
    L"profesora", L"prof.",
    L"marszałka", L"marsz.",
    L"kapitana", L"kpt.",
    L"porucznika", L"por.",
    L"pułkownika", L"płk.",
    L"imienia", L"im.",
    L"numer", L"nr",
    L"świętego", L"św.",
    L"świętej", L"św.",
    L"świętych", L"św.",
    L"błogosławionego", L"bł.",
    L"błogosławionej", L"bł.",
    L"błogosławionych", L"bł.",
    L"kościół", L"kościół",
    L"szkoła podstawowa", L"SP",
    L"liceum ogólnokształcące", L"LO",
    L"liceum", L"LO",
    L"zespół szkół zawodowych", L"ZSZ",
    L"zespół szkół", L"ZS",
    L"pasaż", L"pasaż",
    L"skwer", L"skwer",
    L"ścieżka", L"ścieżka",
    L"pod wezwaniem", L"pw.",
    L"matki boskiej", L"MB",
    L"najświętszej maryi panny", L"NMP",
    L"kanał", L"kan.",
    L"góra", L"g.",
    L"dworzec", L"dworzec",
    L"stacja", L"stacja",
    /* TODO: when skipping "nad" (or German "am") skip until end of string */
    L"nad", L"n.",
    L"główny", L"gł.",
    L"główna", L"gł.", /* TODO: don't touch if first word */
    L"wschodni", L"wsch.",
    L"wschodnia", L"wsch.", /* TODO: don't touch if first word */
    L"zachodni", L"zach.",
    L"zachodnia", L"zach.", /* TODO: don't touch if first word */
    L"pierwszy", L"I",
    L"pierwsza", L"I",
    L"pierwsze", L"I",
    L"drugi", L"II",
    L"druga", L"II",
    L"drugie", L"II",
    L"trzeci", L"III",
    L"trzecia", L"III",
    L"trzecie", L"III",
    L"mazowiecki", L"maz.",
    L"mazowiecka", L"maz.",
    L"mazowieckie", L"maz.",
    L"wielkopolski", L"wlkp.",
    L"wielkopolska", L"wlkp.",
    L"wielkopolskie", L"wlkp.",
    L"śląski", L"śl.",
    L"śląska", L"śl.",
    L"śląskie", L"śl.",
    L"pomorski", L"pom.",
    L"pomorska", L"pom.", /* TODO: don't touch if first word */
    L"pomorskie", L"pom.",
    L"górny", L"g.",
    L"górna", L"g.", /* TODO: don't touch if first word */
    L"górne", L"g.",
    L"dolny", L"d.",
    L"dolna", L"d.", /* TODO: don't touch if first word */
    L"dolne", L"d.",
    L"kolonia", L"kol.",
    L"miasto stołeczne", L"m.st.",
    L"miasta stołecznego", L"m.st.",
    L"braci", L"braci",
    L"rodziny", L"",
    L"pracownicze ogródki działkowe", L"POD",
    L"robotnicze ogródki działkowe", L"ROD",
    L"narodowy fundusz zdrowia", L"NFZ",
    L"spółdzielnia mieszkaniowa", L"SM",
    L"osiedle", L"os.",
    /* TODO: phrases below this line can not be omitted from the shortest
     * form, we need to account for this eventually.  Fortunately they usually
     * come at the end of a name.  */
    L"komisji edukacji narodowej", L"KEN",
    L"polskiego czerwonego krzyża", L"PCK",
    L"armii krajowej", L"AK",
    L"armii ludowej", L"AL",
    L"podziemnej organizacji wojskowej", L"POW",
    L"tysiąclecia", L"1000-lecia",
    L"trzydziestolecia", L"XXX-lecia",
    L"dziesięciolecia", L"X-lecia",
    L"zakład ubezpieczeń społecznych", L"ZUS",
    L"urząd gminy", L"UG",
    L"urząd miasta", L"UM",
    L"gminny ośrodek sportu i rekreacji", L"GOSiR",
    L"miejski ośrodek sportu i rekreacji", L"MOSiR",
    L"ośrodek sportu i rekreacji", L"OSiR",
    L"wojsk ochrony pogranicza", L"WOP",

    /* English */
    L"north", L"n",
    L"east", L"e",
    L"west", L"w",
    L"south", L"s",
    L"northeast", L"ne",
    L"northwest", L"nw",
    L"southeast", L"se",
    L"southwest", L"sw",
    L"street", L"st",
    L"saint", L"st",
    L"state route", L"SR",
    L"state", L"st",
    L"avenue", L"ave",
    L"boulevard", L"blvd",
    L"court", L"ct",
    L"alley", L"aly",
    L"drive", L"dr",
    L"doctor", L"dr.",
    L"junior", L"jr.",
    L"'s", L"",
    L"highway", L"hwy",
    L"route", L"rt",
    L"circle", L"cir",
    L"expressway", L"expy",
    L"loop", L"loop",
    L"parkway", L"pkwy",
    L"national forest service", L"NFS",
    L"national", L"nat",
    L"railroad", L"RR",
    L"right of way", L"RR",
    L"building", L"bldg",
    /* TODO: Some of these are tricky and probably should only be
     * abbreviated when in post position, for example "Bridge Of The Gods"
     * should really stay intact and just disappear when there's not enough
     * space to render the full name.  So let's have a flag that tells us
     * whether something is post-position only.  But post-position doesn't
     * always mean at the end of the string (e.g. the Street in name=Fulton
     * Street North is in post position but not at the end).  Perhaps like
     * in expand.py, treating all the words from the left until a first
     * non-abbrviatable substring is found, as pre-position, and from the
     * right, as in post-position, would be good enough?  Or we could just
     * blacklist "bridge of" as a phrase that only abbreviates to itself
     * and is not discardable.  */
    L"bridge", L"brdg",
    /* TODO: phrases below this line can not be omitted from the shortest
     * form, we need to account for this eventually.  */
    L"martin luther king", L"MLK",
    L"internal revenue service", L"IRS",

    /* Spanish */
    L"calle", L"c.",
    L"avenida", L"av.",
    L"cuesta", L"cuesta",
    L"paseo", L"paseo",
    L"autovía", L"autovia",
    L"autopista", L"autopista",
    L"del", L"",
    L"de", L"",
    L"el", L"",
    L"la", L"",
    L"los", L"",
    L"instituto de educación secundaria", L"IES",
    L"instituto educación secundaria", L"IES",
    L"colegio de educación infantil y primaria", L"CEIP",
    L"colegio educación infantil y primaria", L"CEIP",
    L"colegio público de educación infantil y primaria", L"CEIP",
    L"colegio público educación infantil y primaria", L"CEIP",
    L"colegio público de educación primaria e infantil", L"CEIP",
    L"colegio público educación primaria e infantil", L"CEIP",
    L"doctor", L"dr",
    L"doctora", L"dra",
    L"poeta", L"poeta",
    L"cura", L"cura",
    L"obispo", L"obispo",

    /* German */
    /* TODO: German needs special treatment because the sub-words, in
     * a word formed by concatenation, can be abbreviated individually.  */
    L"straße", L"str.",
    L"strasse", L"str.",
    L"weg", L"weg",
    L"hauptbanhof", L"hbf",

    /* Russian & Ukrainian */
    L"проспе́кт", L"пр.",
    L"проспект", L"пр.",
    L"проезд", L"пр-д",
    L"улица", L"ул.",
    L"вулиця", L"вул.",
    L"бульвар", L"бул.",
    L"майдан", L"майдан",
    L"площа", L"пл.",
    L"площадь", L"пл.",
};

/*
 * Given names in genitive (in many languages this is same as nominative)
 * which should be shortened or omitted from streets named after people.
 */
static const wchar_t *given_names[] = {
    /* Polish */
    L"Abrahama",
    L"Adama",
    L"Adelajdy",
    L"Adolfa",
    L"Adriana",
    L"Agaty",
    L"Agnieszki",
    L"Ahmeda",
    L"Alberta",
    L"Aleksandra",
    L"Aleksandry",
    L"Alfreda",
    L"Alicji",
    L"Alojzego",
    L"Amadeusza",
    L"Ambrożego",
    L"Anastazego",
    L"Anatola",
    L"Andrzeja",
    L"Angeli",
    L"Anieli",
    L"Anny",
    L"Antoniego",
    L"Antoniny",
    L"Apoloniusza",
    L"Arkadiusza",
    L"Arkadego",
    L"Artura",
    L"Azalii",
    L"Baltazara",
    L"Barbary",
    L"Barnaby",
    L"Bartłomieja",
    L"Bartosza",
    L"Bazylego",
    L"Beaty",
    L"Beniamina",
    L"Błażeja",
    L"Bogdana",
    L"Bogumiła",
    L"Bogumiły",
    L"Bolesława",
    L"Bonifacego",
    L"Borysława",
    L"Bruno",
    L"Brunona",
    L"Brygidy",
    L"Celiny",
    L"Cezarego",
    L"Cypriana",
    L"Cyryla",
    L"Czesława",
    L"Czesławy",
    L"Dagmary",
    L"Damiana",
    L"Daniela",
    L"Danuty",
    L"Darii",
    L"Dariusza",
    L"Dawida",
    L"Dionizego",
    L"Dominika",
    L"Dominiki",
    L"Donalda",
    L"Doroty",
    L"Edmunda",
    L"Edwarda",
    L"Edwina",
    L"Edyty",
    L"Emila",
    L"Emiliusza",
    L"Emiliana",
    L"Eryka",
    L"Eugeniusza",
    L"Eustachego",
    L"Euzebii",
    L"Eweliny",
    L"Ewy",
    L"Fabiana",
    L"Feliksa",
    L"Felicjana",
    L"Filipa",
    L"Floriana",
    L"Franciszka",
    L"Fryderyka",
    L"Gabriela",
    L"Gawła",
    L"Genowefy",
    L"Geralda",
    L"Gerwazego",
    L"Grażyny",
    L"Grety",
    L"Grzegorza",
    L"Gustawa",
    L"Haliny",
    L"Hanny",
    L"Hektora",
    L"Helmuta",
    L"Henryka",
    L"Herberta",
    L"Hermenegildy",
    L"Hieronima",
    L"Hilarego",
    L"Hipolita",
    L"Honoraty",
    L"Huberta",
    L"Hugo",
    L"Hugona",
    L"Ignacego",
    L"Igor",
    L"Ildefonsa",
    L"Indiry",
    L"Ireneusza",
    L"Ireny",
    L"Iwo",
    L"Iwony",
    L"Izydora",
    L"Jacka",
    L"Jadwigi",
    L"Jagny",
    L"Jagody",
    L"Jakuba",
    L"Jana",
    L"Janusza",
    L"Jarosława",
    L"Jaśminy",
    L"Jeremiasza",
    L"Jeremiego",
    L"Jerzego",
    L"Jędrzeja",
    L"Joahima",
    L"Johana",
    L"Jonasza",
    L"Jolanty",
    L"Józefa",
    L"Juliana",
    L"Julii",
    L"Juliusza",
    L"Juranda",
    L"Justyny",
    L"Kacpra",
    L"Kajetana",
    L"Kaji",
    L"Kamila",
    L"Kalasantego",
    L"Karola",
    L"Karoliny",
    L"Katarzyny",
    L"Kazimierza",
    L"Kingi",
    L"Klaudiusza",
    L"Kleofasa",
    L"Konrada",
    L"Konstantego",
    L"Kornela",
    L"Krystiana",
    L"Krystyny",
    L"Krzysztofa",
    L"Ksawerego",
    L"Lecha",
    L"Leny",
    L"Leokadii",
    L"Leona",
    L"Leonida",
    L"Leszka",
    L"Lidii",
    L"Lucjana",
    L"Ludwika",
    L"Ludwiki",
    L"Ludomiły",
    L"Łazarza",
    L"Łucji",
    L"Łukasza",
    L"Macieja",
    L"Maji",
    L"Maksymiliana",
    L"Malwiny",
    L"Małgorzaty",
    L"Marcelego",
    L"Marceliny",
    L"Marcina",
    L"Marii",
    L"Marianny",
    L"Marioli",
    L"Mariusza",
    L"Marleny",
    L"Marka",
    L"Marty",
    L"Martyny",
    L"Marzeny",
    L"Mateusza",
    L"Matyldy",
    L"Maurycego",
    L"Melanii",
    L"Michała",
    L"Michaliny",
    L"Mieczysława",
    L"Mieczysławy",
    L"Mikołaja",
    L"Miłosza",
    L"Mirona",
    L"Mirosława",
    L"Moniki",
    L"Natalii",
    L"Nikodema",
    L"Norberta",
    L"Ofelii",
    L"Olafa",
    L"Olgi",
    L"Olgierda",
    L"Oliwii",
    L"Onufrego",
    L"Oskara",
    L"Otylii",
    L"Pafnucego",
    L"Pankracego",
    L"Patrycji",
    L"Patryka",
    L"Pauliny",
    L"Pawła",
    L"Piotra",
    L"Porfirego",
    L"Prota",
    L"Protazego",
    L"Przemysława",
    L"Radosława",
    L"Rafała",
    L"Rajmunda",
    L"Remigiusza",
    L"Roberta",
    L"Rolanda",
    L"Romana",
    L"Romualda",
    L"Rudolfa",
    L"Ryszarda",
    L"Sabiny",
    L"Samuela",
    L"Sandry",
    L"Saszy",
    L"Sebastiana",
    L"Sergiusza",
    L"Seweryna",
    L"Sławomira",
    L"Sławomiry",
    L"Sobiesława",
    L"Stanisława",
    L"Stefana",
    L"Stefanii",
    L"Sylwestra",
    L"Szczepana",
    L"Szymona",
    L"Tadeusza",
    L"Teodora",
    L"Teofila",
    L"Teresy",
    L"Tobiasza",
    L"Tomasza",
    L"Tymona",
    L"Tymoteusza",
    L"Tytusa",
    L"Urszuli",
    L"Wacława",
    L"Waldemara",
    L"Walentego",
    L"Weroniki",
    L"Wiesława",
    L"Wiesławy",
    L"Wiktora",
    L"Wiktorii",
    L"Wincentego",
    L"Wisławy",
    L"Wilhelma",
    L"Wioletty",
    L"Witolda",
    L"Włodzimierza",
    L"Wolfganga",
    L"Zachariasza",
    L"Zbigniewa",
    L"Zdzisława",
    L"Zdzisławy",
    L"Zenobii",
    L"Zenobiusza",
    L"Zenona",
    L"Zofii",
    L"Zygfryda",
    L"Zygfrydy",
    L"Zygmunta",
};

static locale_t l;

void utf_init(void)
{
    l = newlocale(LC_ALL_MASK, "C.UTF-8", NULL);
#ifdef linux
    uselocale(l);
#endif
}

void utf_done(void)
{
    freelocale(l);
    l = NULL;
}

void shorten_name(const char *name,
		char short_name[512], char shortest_name[512])
{
    wchar_t w_name[512];
    wchar_t w_short_name[512];
    wchar_t w_shortest_name[512];

    const wchar_t *cur_word, *wchar_ptr;
    wchar_t *cur_short_word, *cur_shortest_word;

    int unabbrev = 0;
    int i, len, new_len, capital;

    if (!name)
        return;

    mbsrtowcs_l(w_name, &name, ARRAY_SIZE(w_name), NULL, l);

    /* TODO: also skip anything in parenthesis from the short names */

    /* TODO: instead of calling wcscasecmp_l all the time it'd be more
     * effective to lower case w_name once and use plain wcscmp or mem
     * compare since the phrases we search for are all lower case already.
     * Might also want to take "collation" into account (wcsxfrm_l the
     * string once and use memcmp instead of wcscoll_l).
     */

    cur_word = w_name;
    cur_short_word = w_short_name;
    cur_shortest_word = w_shortest_name;
    while (1) {
        while (*cur_word && !iswalnum_l(*cur_word, l))
	    *cur_short_word ++ = *cur_shortest_word ++ = *cur_word ++;

	if (!*cur_word)
	    break;

        /* TODO: use a hash of some kind instead of iterating over arrays */

        /* Go through possible abbreviations from top to bottom */
        for (i = 0; i < ARRAY_SIZE(abbrevs); i += 2)
	    if (!wcsncasecmp_l(abbrevs[i], cur_word, wcslen(abbrevs[i]), l)) {
                len = wcslen(abbrevs[i]);

	        /* Check that we matched a full word */
                if (iswalnum_l(cur_word[len], l))
		    continue;

		capital = iswupper_l(*cur_word, l);
		cur_word += len;

		new_len = wcslen(abbrevs[i + 1]);
		memcpy(cur_short_word, abbrevs[i + 1],
		        new_len * sizeof(wchar_t));
		/*
		 * If original was capitalised then capitalise the abbreviation
		 * as well, if it was lower case.
		 */
		if (capital)
		    *cur_short_word = towupper_l(*cur_short_word, l);

		/* Make sure shortest_word doesn't end up being empty */
		if (!*cur_word && !unabbrev) {
		    memcpy(cur_shortest_word, cur_short_word,
		            new_len * sizeof(wchar_t));
		    cur_shortest_word += new_len;
		}

		cur_short_word += new_len;

		/*
		 * Avoid excess whitespace in short and shortest
		 * when a word is replaced with "".
		 * TODO: this may require more complicated logic to get
		 * the corner cases right.
		 */
		if (new_len == 0) {
		    if (cur_short_word > w_short_name &&
		            iswspace_l(cur_short_word[-1], l))
			cur_short_word --;
		    if (cur_shortest_word > w_shortest_name &&
		            iswspace_l(cur_shortest_word[-1], l))
			cur_shortest_word --;
	        }

                /*if (new_len != len)*/
		break;
	    }
        if (i < ARRAY_SIZE(abbrevs))
	    continue;

        /* Go through possible given names from top to bottom */
        for (i = 0; i < ARRAY_SIZE(given_names); i ++)
	    if (!wcsncasecmp_l(given_names[i], cur_word,
	            wcslen(given_names[i]), l)) {
                len = wcslen(given_names[i]);

	        /* Check that we matched a full word */
                if (iswalnum_l(cur_word[len], l))
		    continue;

		/*
		 * If this is the final part of the name, and it matches a
		 * given name then that's most likely somebody's surname which
		 * happens to also be a possibble given name.  In that case
		 * do not abbreviate or omit it.
		 */
		if (!cur_word[len])
		    continue;

		cur_word += len;

		*cur_short_word++ = given_names[i][0];
		*cur_short_word++ = L'.';

		/*
		 * Avoid excess whitespace in shortest when a word is
		 * replaced with "".
		 * TODO: this may require more complicated logic to get
		 * the corner cases right.
		 */
		if (cur_shortest_word > w_shortest_name &&
		        iswspace_l(cur_shortest_word[-1], l))
		    cur_shortest_word --;

		break;
	    }
        if (i < ARRAY_SIZE(given_names))
	    continue;

        /* Nothing matched, copy the current word as-is */
        while (iswalnum_l(*cur_word, l))
	    *cur_short_word ++ = *cur_shortest_word ++ = *cur_word ++;
	unabbrev += 1;
    }

    *cur_short_word = 0;
    *cur_shortest_word = 0;

    wchar_ptr = w_short_name;
    wcsrtombs_l(short_name, &wchar_ptr, 512, NULL, l);
    wchar_ptr = w_shortest_name;
    wcsrtombs_l(shortest_name, &wchar_ptr, 512, NULL, l);
}
