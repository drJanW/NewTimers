#include "SayTime.h"
#include "PlaySentence.h"
#include "Globals.h"
#include "ContextManager.h"

static inline void speak(const String &s)
{
    if (!isWiFiConnected())
    {
        PF("[TTS] No WiFi: %s\n", s.c_str());
        return;
    }
    PlaySentence::startTTS(s);
}

static String periodOfDay(uint8_t hour)
{
    if (hour < 6)
        return " snachts";
    if (hour < 12)
        return " sochtends";
    if (hour < 18)
        return " smiddags";
    return " savonds";
}
static String periodPhrase(uint16_t mins)
{
    // 00:00–05:29 nacht, 05:30–08:20 ochtend, 08:21–16:29 stil (geen toevoeging),
    // 16:30–17:59 middag, 18:00–23:59 avond
    if (mins < 330)
        return " snachts";
    if (mins < 501)
        return " smorgens";
    if (mins < 990)
        return ""; 
    if (mins < 1080)
        return " smiddags";
    return " savonds";
}

static String buildTimeText(uint8_t hour24, uint8_t minute, TimeStyle style)
{
    hour24 %= 24;
    minute %= 60;

    uint8_t baseHour12 = hour24 % 12;
    if (baseHour12 == 0)
        baseHour12 = 12;
    String sBaseHour12 = String(baseHour12);
    uint8_t nextHour12 = (baseHour12 % 12) + 1;
    String sNextHour12 = String(nextHour12);



    String t;
    switch (style)
    {
    case TimeStyle::FORMAL:
    {
        // “HH uur” / “HH uur MM”
        t.reserve(24);
        t += String(hour24);
        t += " uur";
        if (minute > 0)
        {
            t += " ";
            t += String(minute);
        }
        break;
    }

    case TimeStyle::NORMAL:
    {
        if (minute == 0)
        {
            t =sBaseHour12 + " uur";
        }
        else if (minute == 15)
        {
            t = "kwart over " +sBaseHour12;
        }
        else if (minute == 30)
        {
            t = "half " + sNextHour12;
        }
        else if (minute == 45)
        {
            t = "kwart voor " + sNextHour12;
        }
        else if (minute < 30)
        {
            t = String(minute) + " over " +sBaseHour12;
        }
        else
        {
            uint8_t diff = 60 - minute;
            t = String(diff) + " voor " + sNextHour12;
        }
        t += periodOfDay(hour24);
        break;
    }

    case TimeStyle::INFORMAL:
    {
        uint8_t hour12 = hour24 % 12;
        if (hour12 == 0)
            hour12 = 12;
        uint8_t nextHour = (hour12 % 12) + 1;
        uint16_t mins = hour24 * 60 + minute;
        String pfx; // willekeurige voorzetjes
        if ((11 + minute) % 37 == 0)
            pfx += (pfx.isEmpty() ? "" : " "), pfx += "volgens mij";
        if ((23 + minute) % 39 == 0)
            pfx += (pfx.isEmpty() ? "" : " "), pfx += "iets van";
        if ((37 + minute) % 43 == 0)
            pfx += (pfx.isEmpty() ? "" : " "), pfx += "zo’n beetje";

        const String per = periodPhrase(mins);
        auto add = [&](const String &s)
        { if (!t.isEmpty()) t += " "; t += s; };

        if (!pfx.isEmpty())
            add(pfx);

        switch (minute)
        {
        case 0:
            if ((hour12 + mins) % 3 == 0)
                t = String("exact ") + String(hour12) + " uur" + per;
            else
                t = String(hour12) + " uur" + per + " precies";
            break;

        case 1:
            if ((hour12 + mins) % 11 == 0)
                t = String("alweer ") + String(hour12) + " uur" + per + " geweest";
            else
                t = String("iets over ") + String(hour12) + " uur" + per;
            break;

        case 2:
            t = String("ruim ") + String(hour12) + " uur" + per + " geweest";
            break;

        case 3 ... 12:
            if ((hour12 + mins) % 3 == 0)
                t = String(minute) + " minuten over " + String(hour12) + per;
            else
                t = String(minute) + " over " + String(hour12) + per;
            break;

        case 13:
            t = String("nog geen kwart over ") + String(hour12) + per;
            break;

        case 14:
            t = String("plusminus kwart over ") + String(hour12) + per;
            break;

        case 15:
            t = String("kwart over ") + String(hour12) + per + " precies";
            break;

        case 16:
            t = String("zo’n kwart over ") + String(hour12) + per;
            break;

        case 17:
            t = String("ruim kwart over ") + String(hour12) + per + " geweest";
            break;

        case 18 ... 19:
            if ((hour12 + mins) % 13 == 0)
                add("nu zo’n beetje");
            t = String(minute) + " minuten over " + String(hour12) + per;
            break;

        case 20 ... 27:
        {
            if ((hour12 + mins) % 19 == 0)
                add("nu zo’n beetje");
            uint8_t diff = 30 - minute;
            t = String(diff) + " minuten voor half " + String(nextHour) + per;
            break;
        }

        case 28:
            t = String("nog geen half ") + String(nextHour) + per;
            break;

        case 29:
            t = String("bijna half ") + String(nextHour) + per;
            break;

        case 30:
            if ((hour12 + mins) % 7 == 0)
                t = String("half ") + String(nextHour) + per + " precies";
            else
                t = String("exact ") + String(nextHour) + per;
            break;

        case 31:
            t = String("om en nabij half ") + String(nextHour) + per;
            break;

        case 32:
            t = String("ruim half ") + String(nextHour) + per;
            break;

        case 33 ... 39:
        {
            uint8_t diff = minute - 30;
            t = String(diff) + " over half " + String(nextHour) + per;
            if ((hour12 + mins) % 13 == 0)
                t += " precies";
            break;
        }

        case 40 ... 42:
        {
            if ((hour12 + mins) % 17 == 0)
                add("nu zo’n beetje");
            uint8_t diff = minute - 30;
            t = String(diff) + " over half " + String(nextHour) + per;
            break;
        }

        case 43:
            t = String("zowat kwart voor ") + String(nextHour) + per;
            break;

        case 44:
            t = String("net geen kwart ") + String(nextHour) + per;
            break;

        case 45:
            t = String("kwart voor ") + String(nextHour) + " precies" + per;
            break;

        case 46:
            t = String("zo’n beetje kwart voor ") + String(nextHour) + per + " geweest";
            break;

        case 47:
            t = String("iets van kwart voor ") + String(nextHour) + per;
            break;

        case 48 ... 54:
        {
            if ((hour12 + mins) % 21 == 0)
                add("alweer");
            uint8_t diff = 60 - minute;
            if ((hour12 + mins) % 3 == 0)
                t = String(diff) + " minuten voor " + String(nextHour) + per;
            else
                t = String(diff) + " voor " + String(nextHour) + per;
            break;
        }

        case 55:
            t = String("5 voor ") + String(nextHour) + per;
            break;

        case 56:
            t = String("nog geen ") + String(nextHour) + " uur" + per;
            break;

        case 57:
            t = String("bijna ") + String(nextHour) + " uur" + per;
            break;

        case 58:
            t = String("plusminus ") + String(nextHour) + " uur" + per;
            break;

        case 59:
            t = String("tegen de klok van ") + String(nextHour) + " uur" + per;
            break;
        }
        break;
    }
    }
    return t;
}

static const char *monthName(uint8_t m)
{
    static const char *names[12] = {
        "januari", "februari", "maart", "april", "mei", "juni",
        "juli", "augustus", "september", "oktober", "november", "december"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "";
}

static const char *weekdayName(uint8_t dow)
{
    // aannemende: 0=zondag (zoals je eerdere code impliceert)
    static const char *days[7] = {
        "zondag", "maandag", "dinsdag", "woensdag", "donderdag", "vrijdag", "zaterdag"};
    return (dow < 7) ? days[dow] : "";
}

static String buildDateText(uint8_t day, uint8_t month, uint8_t year, TimeStyle style)
{
    String d;
    d.reserve(32);
    if (day >= 1 && day <= 31)
    {
        d += String(day);
        d += " ";
    }
    d += monthName(month);
    if (year > 0)
    {
        if (style == TimeStyle::INFORMAL)
        {
            // vaak spreek je het jaar niet; maar indien gewenst:
            // d += ", " + String(2000 + year); (commentaar)
        }
        else
        {
            d += " ";
            // Als je 2-cijferig jaar aanlevert, laat TTS “25” prima uitspreken; 0 = geen jaar
            d += String(year < 100 ? (2000 + year) : year);
        }
    }
    return d;
}

void SayTime(uint8_t hour, uint8_t minute, TimeStyle style)
{
    speak(buildTimeText(hour, minute, style));
}

void SayDate(uint8_t day, uint8_t month, uint8_t year, TimeStyle style)
{
    speak(buildDateText(day, month, year, style));
}

void SayNow(TimeStyle style)
{
    const auto &timeCtx = ContextManager::time();
    String sentence = "Het is ";
    sentence += weekdayName(timeCtx.dayOfWeek);
    sentence += " ";
    sentence += buildDateText(timeCtx.day, timeCtx.month, static_cast<uint8_t>(timeCtx.year - 2000), style);
    sentence += ", ";
    sentence += buildTimeText(timeCtx.hour, timeCtx.minute, style);
    speak(sentence);
}
