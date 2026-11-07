# CITY LIFE 3D – Welt- und Ortskonzept

## Grundidee

CITY LIFE 3D soll mehrere unterschiedliche Orte bieten. Die Welt besteht nicht aus einer einzigen riesigen Karte, sondern aus auswählbaren Regionen und eigenständigen, für den Nintendo 3DS optimierten Spielgebieten.

## Die drei Wunschstädte

1. **Bayreuth (Deutschland)** – kleinere Stadt mit Altstadt, Wohnvierteln, Parks und Straßen.
2. **Tokio (Japan)** – dicht bebaute Stadt mit Hochhäusern, engen Nebenstraßen, Leuchtreklamen und großen Kreuzungen.
3. **Los Angeles (USA)** – breite Straßen, Palmen, Strandnähe und Hochhäuser.

## Weitere Wunschregionen

- **Strandregion** – Küstenstraße, Sandstrand, Promenade und Meer.
- **Gebirgsregion** – Berge, Serpentinen, Täler, Aussichtspunkte und kleine Bergorte.
- **Historisches Rom / Mittelalter-Themengebiet** – eine frei erfundene historische Spielregion mit Kopfsteinpflaster, alten Plätzen, Stadtmauern, Türmen und Marktplatz. Keine moderne Stadtkarte; bewusst als eigene, stilisierte historische Welt gestaltet.

## Perspektiven und Spielgefühl

- Dritte-Person-Kamera hinter der Spielfigur bzw. dem Fahrzeug als Hauptansicht.
- Zusätzliche Kameraperspektiven, z. B. näher an der Figur und eine Fahrzeugansicht.
- Orte sollen sich optisch und atmosphärisch deutlich unterscheiden.
- Spieler soll zwischen den Orten im Hauptmenü wählen können.


## Lebendige Spielwelt und Interaktionen

### Läden und Einkaufen
- In Bayreuth soll es einen begehbaren Discounter geben, der sich wie ein typischer Supermarkt anfühlt. Für das Spiel verwenden wir zunächst einen eigenen Fantasienamen und eigene Gestaltung statt geschützter Markenlogos.
- Regale, Kassen, Einkaufskörbe und Waren als einfache, erkennbare 3D-Objekte.
- Spieler kann Gegenstände ansehen, kaufen und im Inventar ablegen.
- Später können weitere Läden wie Kiosk, Fahrradladen, Café und Kleidungsgeschäft hinzukommen.


### Geparkte Fahrzeuge und freie Fahrzeugnutzung
- In den Straßen und auf Parkplätzen stehen verschiedene Fahrzeuge: Autos, Fahrräder, E-Scooter, Motorroller und Motorräder.
- Fahrzeuge sind als eigene, stilisierte Low-Poly-Modelle erkennbar und passend zu den jeweiligen Regionen gestaltet.
- Der Spieler kann sich Fahrzeuge ansehen, an ihnen eine Interaktion auslösen und – sofern sie im Spiel freigegeben sind – einsteigen und losfahren.
- Jedes Fahrzeug bekommt ein einfaches, unterscheidbares Fahrgefühl: Autos stabiler, Scooter und Roller wendiger, Motorräder schneller und empfindlicher beim Lenken.
- Für den Anfang gibt es wenige Fahrzeuge in einem kleinen Testgebiet; später kommen mehr Varianten und Parkplätze hinzu.
- Fahrzeuge werden nicht alle gleichzeitig aktiv simuliert. Nicht benötigte Fahrzeuge bleiben einfache Dekoration, damit Speicher und Leistung des Nintendo 3DS geschont werden.
- Fahrzeugnutzung ist reine Spielmechanik in einer fiktiven Spielwelt und keine Anleitung für das Fahren echter Fahrzeuge.

### NPCs und Stadtleben
- Unterschiedliche Passanten mit variierenden Farben und Outfits.
- NPCs bewegen sich auf Gehwegen, bleiben gelegentlich stehen und wechseln einfache Wegpunkte.
- Kunden betreten Läden; Verkäufer stehen an der Kasse.
- Einfache optionale Dialoge und kleine Aufgaben, z. B. etwas ausliefern oder einen Treffpunkt finden.
- NPC-Verhalten wird bewusst leichtgewichtig gehalten, damit der 3DS nicht überlastet wird.

### Aktionen und Spielregeln
- Türen öffnen, mit Figuren sprechen, Gegenstände aufnehmen, einkaufen und Fahrzeuge nutzen.
- Neben normalen Aktivitäten kann es harmlose, cartoonhafte Quatsch-Missionen geben. Diese bleiben klar fiktiv und spielerisch; keine realistischen Anleitungen zu Einbruch, Werkzeuggebrauch oder dem Umgehen echter Sicherheitssysteme.
- Falls eine Mission unerlaubtes Mitnehmen eines erfundenen Spielgegenstands enthält, wird das abstrakt und nicht realistisch dargestellt, mit klaren Spielkonsequenzen und ohne echte Methoden zu erklären.

### Umsetzungsreihenfolge
1. Einen kleinen begehbaren Straßenabschnitt mit Spielfigur und Kamera bauen.
2. Ein Gebäude mit Tür und einem einfachen Innenraum ergänzen.
3. Ein Geschäft mit Regalen, Waren, Kaufen-Interaktion und Inventar erstellen.
4. Wenige NPCs mit einfachen Laufwegen hinzufügen und Leistung auf dem 3DS testen.
5. Erst danach weitere Läden, Missionen und Stadtviertel ausbauen.

## Technische Umsetzung

- Kein direktes, dauerhaftes Google-Maps-Streaming auf dem 3DS.
- Wo sinnvoll, können offene Kartendaten wie OpenStreetMap als grobe Vorlage für Straßen und Gebäude dienen. Daten und Lizenzbedingungen müssen vor einer Verwendung geprüft werden.
- Spielgebiete werden offline vorbereitet, vereinfacht und in kleinere Abschnitte aufgeteilt, damit sie auf der begrenzten Hardware des Nintendo 3DS laufen.
- Die historischen, Strand- und Gebirgsgebiete werden als eigene stilisierte Spielwelten gebaut.
- Erst einen kleinen stabilen 3D-Testbereich umsetzen, dann die drei Städte und zusätzlichen Regionen schrittweise ergänzen.

## Wichtige Qualitätsziele

- Bunte, freundliche Low-Poly-/Cartoon-Optik.
- Stabile Darstellung ohne Flackern oder schwarze Grafikfehler.
- Gute Bildrate und kurze Ladezeiten.
- Keine kostenpflichtigen Dienste oder Assets.
- Alle Grafiken und Sounds selbst erstellen oder korrekt lizenzierte freie Inhalte verwenden.

## Status und verbindliche Meilensteine

Der aktuelle Stand ist ein früher Szenenprototyp mit D-Pad-Bewegung, einem einfachen Spieler, laufenden Passanten sowie einem Roller und einem kompakten Auto als einsteigbaren Fahrzeug-Prototypen. Wichtig: Die derzeitige Darstellung verwendet noch flache, bildschirmorientierte Geometrie und eine einfache Drehtransformation. Sie ist daher noch keine echte räumliche 3D-Kamera und darf nicht als fertige 3D-Stadt bezeichnet werden. Mit A kann der Spieler in die Nähe eines Fahrzeugs gehen, einsteigen und es mit dem Steuerkreuz bewegen; beim Aussteigen bleibt es an der neuen Position. Die CI-Pipeline hat einen Build erfolgreich kompiliert, als CIA verpackt und eine GitHub-Release-Datei veröffentlicht. Ein Test auf echter 3DS-Hardware ist weiterhin offen; Darstellung, Steuerung und Leistung müssen dort geprüft werden.

### Reihenfolge bis zur ersten spielbaren CIA
1. Build-Pipeline zuverlässig kompilieren und eine gültige CIA erzeugen.
2. Spielfigur, Eingabesteuerung, Kollisionsgrenzen und Kamera in einem kleinen Testgebiet implementieren.
3. Echte 3D-Grundlage aufbauen: räumliche X/Y/Z-Geometrie, Perspektiv-Projektionsmatrix, View-Matrix mit Kamera hinter der Figur bzw. dem Fahrzeug, Tiefentest und Kamera-relative Bewegung. Für die Implementierung sind die Citro3D-Matrixfunktionen `Mtx_Persp` und `Mtx_LookAt` in der offiziellen `maths.h`-Schnittstelle vorhanden. Touch-Drag verändert anschließend Yaw und Pitch der Kamera. Erst den kleinen 3D-Testbereich bauen und kompilieren, bevor die Stadt erweitert wird.
4. Fahrzeug-Prototyp mit Ein-/Aussteigen und einfacher Fahrphysik ergänzen – erste Bewegungs-Prototypen mit Roller und Auto sind im Code; anschließend auf echter Hardware testen.
5. Ein begehbares Geschäft mit Regalen, Kaufaktion und einfachem Inventar bauen.
6. Wenige NPCs mit einfachen Wegpunkten und Dialogen ergänzen.
7. Erst nach stabilen Tests die weiteren Stadtviertel und auswählbaren Welten ausbauen.

Jeder Meilenstein muss auf echter 3DS-Hardware oder in einem geeigneten Testlauf geprüft werden. Eine erfolgreiche Kompilierung allein bedeutet noch nicht, dass das Spiel auf dem 3DS korrekt läuft.


### Technische Vorarbeit nach Build 23
- Build 23 wurde am 7. November 2026 von GitHub Actions erfolgreich gebaut und als `CITY-LIFE-3D.cia` veröffentlicht. Die Änderung an der Bildausgabe ist kompiliert, aber noch nicht auf echter Hardware bestätigt.
- Die offizielle Citro3D-Schnittstelle wurde geprüft: `Mtx_PerspTilt` erzeugt die perspektivische Projektion für den oberen 3DS-Bildschirm; `Mtx_LookAt` erzeugt die Kamera-View-Matrix; `FVec3_New` erstellt die benötigten 3D-Vektoren.
- Für die Tiefenverdeckung ist `C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL)` bzw. die passende Vergleichsrichtung anhand des verwendeten Tiefenformats zu prüfen; die konkrete Konfiguration muss vor dem ersten 3D-Test gegen ein offizielles Citro3D-Beispiel abgeglichen werden. Flächen-Culling bleibt zunächst deaktiviert, bis die Dreiecksreihenfolge auf dem Gerät bestätigt ist.
- Der nächste technische Prototyp soll nur eine kleine Testfläche enthalten: Boden, zwei unterschiedlich hohe Quader, eine einfache Spielfigur und eine Kamera hinter der Figur. Die Kamera wird aus Yaw/Pitch und Abstand berechnet; Touch-Drag ändert ausschließlich diese Winkel. Die Spielfigur bewegt sich relativ zur Kamera, nicht durch Drehen der ganzen Welt.
- Erst wenn dieser Test sichtbar korrekt ist, werden Stadtgebäude, NPCs und Fahrzeuge auf räumliche X/Y/Z-Modelle umgestellt. Die bisherige flache Szene bleibt bis dahin ausdrücklich ein Prototyp.
