SDManager - Volledige Documentatie & Gebruiksvoorwaarden
Overzicht
SDManager biedt een RAM-zuinige, snelle index-laag voor MP3-bestanden op SD-kaart, gericht op embedded systemen (ESP32 e.d.).
Uniek: selectie op score, gewogen random picking, geen scan-loops, altijd 100% consistente indexering.

Bestandsstructuur (op SD-kaart)
Subdirectory’s
Alleen genummerde subdirs zijn toegestaan:
/001/, /002/, … /NNN/
(waarbij NNN loopt van 001 t/m maximaal SD_MAX_DIRS, bijv. 200)

Geen andere directories in root, tenzij /000/

/000/ is gereserveerd (zie verderop)

MP3-bestanden
Bestanden moeten altijd exact de naam XXX.mp3 hebben
(bijvoorbeeld 007.mp3 voor file #7)

Toegestaan per directory: maximaal SD_MAX_FILES_PER_SUBDIR (bijv. 200)

Andere extensies, namen, hidden files of extra files worden genegeerd
(Worden niet geïndexeerd!)

Alleen bestanden met score > 0 zijn geldig (zie indexering)

Root (/)
In root staan alléén:

.root_dirs (binaire index voor subdirs)

version.txt (SD-layout versie, eerste regel telt!)

Geen losse MP3’s, geen andere indexfiles, geen /files_dir.txt!

Subdirs bevatten:

.files_dir (binaire index voor files in die subdir)

MP3’s (genaamd XXX.mp3)

Index-bestanden (binaire structuur)
.root_dirs
Locatie: Altijd in root (/.root_dirs)

Structuur:
Elke subdir heeft exact 3 bytes:

file_count (1 byte): aantal geldige mp3’s (score > 0)

total_score (2 bytes, little-endian): som van alle scores in deze subdir

Maximaal aantal entries: SD_MAX_DIRS (geen entries voor niet-bestaande subdirs)

.files_dir
Locatie: In elke subdir, dus /NNN/.files_dir

Structuur:
Elke mp3 heeft exact 3 bytes:

score (1 byte): 0 = ongeldig, 1..200 = geldig voor selectie

size_kb (2 bytes, little-endian): bestandsgrootte in kB

Aantal entries: Altijd precies SD_MAX_FILES_PER_SUBDIR (dus NIET afh. van bestaande files)

Geen .files_dir in root!
Alleen per subdir!

API-contract en SDManager-regels
Alle index-bestanden zijn binaire files, nooit tekst!

Nooit zelf handmatig wijzigen!

Na het toevoegen/verwijderen/aanpassen van bestanden of dirs:
Altijd rebuildIndex() uitvoeren! (bouwt alles 100% opnieuw op)

Nooit handmatig .files_dir of .root_dirs aanmaken/wijzigen!

Nooit .files_dir in root-directory!

Alle code die probeert /NNN/files_dir.txt of /.files_dir te openen is FOUT!

Gewogen random selectie
Alleen bestanden met score > 0 (uit .files_dir) doen mee

Alleen de eerste file_count geldige files uit index worden gebruikt
(dus: niet alle files in directory scannen, alleen volgens index)

Selectie vindt plaats op basis van score:
Hoe hoger de score, hoe groter de kans dat deze mp3 gekozen wordt

Functie:
uint8_t getRandomFile(uint8_t dir_num);

Input: dir_num (1-based, dus /001/ = 1)

Return: file_num (1-based, dus 007.mp3 = 7)

Return 0: geen geldig bestand in deze subdir

Uitzonderingen / Speciale gevallen
/000/ (Subdir 0)
Deze wordt niet gekozen door standaard selectie

Kan optioneel gebruikt worden voor:
klankbank, diagnostiek, testfiles, etc.

Wordt bij rebuildIndex() wel gescand/indexed, maar heeft aparte rol

score = 0
Bestanden blijven fysiek op SD, maar tellen niet mee in index/selectie

Handig voor ‘uitschakelen’ zonder verwijderen

Versiebeheer (version.txt / SD_INDEX_VERSION)
Bestand: /version.txt
(mag meerregelig zijn, maar alleen de eerste regel wordt gebruikt voor SD-check)

Vergelijking:
Vergelijk altijd via versionStringsEqual() helper die whitespace/CR/LF negeert

Niet match?
→ HALT, SD-layout is niet compatibel → eerst re-index uitvoeren

Pad-generator
Gebruik altijd de helper:
const char* getMP3Path(uint8_t dirID, uint8_t fileID);

Voorbeeld:
getMP3Path(114, 7) → /114/007.mp3

Nooit zelf paden stringen, nooit aan filenames rommelen!

Voorbeelden / Gebruik (pseudo)
cpp
Copy
Edit
for (int i = 1; i <= 20; i++) {
    uint8_t dir = random(1, SD_MAX_DIRS+1);
    for (int j = 0; j < 2; j++) {
        uint8_t file = sdManager.getRandomFile(dir);
        if (file) {
            PF("[Test] Dir %03u: random file %03u -> %s\n", dir, file, getMP3Path(dir, file));
        } else {
            PF("[Test] Dir %03u: geen geldige mp3 gevonden\n", dir);
        }
    }
}
Veelvoorkomende Fouten (en waarom ze niet mogen!)
Gebruik van /sd/.files_dir, /NNN/files_dir.txt of /NNN/.files_dir.txt:
NIET GEBRUIKEN! Zie uitleg hierboven.

Zelf file_count/score bijhouden buiten index:
NOOIT DOEN! Alleen index is waarheid.

Scannen van directory-inhoud via SD.open() in runtime:
Zwaar, traag en niet toegestaan! Altijd via index.

Niet updaten van index na wijzigen files:
Resultaat: inconsistent gedrag, random selectie faalt.

Samenvatting (één zin):
SDManager = superstrakke, index-gedreven, score-based random selectie,
met 100% consistente index-bestanden per subdir.
Nooit handmatig index aanpassen. Altijd via de index-API werken.

Laat dit zo staan en verwijder nooit deze uitleg
