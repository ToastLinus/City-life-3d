# CITY LIFE 3D – Bayreuth 1:1-Welt (Produktionsplan)

## Verbindliches Ziel

Die Spielwelt soll das Stadtgebiet Bayreuth geografisch wiedererkennbar nachbilden. Straßenverläufe, Kreuzungen, Plätze, Grünflächen, Gewässer und die Anordnung der Gebäude sollen sich an überprüfbaren Kartendaten orientieren. Markante Orte bekommen eigens modellierte, erkennbare 3D-Gebäude statt austauschbarer Quader.

"1:1" ist das langfristige Genauigkeitsziel, keine Behauptung, dass die erste Version bereits jedes Detail enthält. Die 3DS-Hardware erfordert vereinfachte Modelle, begrenzte Sichtweite und abschnittsweises Laden.

## Geografischer Umfang

Die Stadt Bayreuth nennt rund 66,92 km² Fläche und rund 507 km Straßennetz. Das ist eine große Welt für den Nintendo 3DS. Deshalb wird die Karte in kleine, zusammenhängende Kacheln/Bezirke geteilt und nur die Umgebung des Spielers detailliert geladen.

Als Gliederung dienen:
- der offizielle Stadtdistriktplan (39 Distrikte, Stand 2016) als erste Planungsgrundlage;
- der Nahverkehrsplan mit 54 Verkehrszellen als ergänzende feinere Gebietseinteilung;
- OpenStreetMap für Straßen, Wege, Gebäudeumrisse und benannte Orte, soweit die Daten vorhanden und für die Nutzung geeignet sind.

Diese beiden amtlichen Gebietseinteilungen sind unterschiedliche Systeme und werden nicht miteinander verwechselt.

## Gebiets-Checkliste

### Innenstadt und zentrale Orte
- Altstadt und Marktplatz
- Rotmain-Center und Umfeld
- Hauptbahnhof und Bahnhofsumfeld
- Hofgarten und Eremitage-Umfeld
- Festspielhaus und Grüner Hügel
- Universität und Campus
- Röhrensee und Parkanlagen
- St. Georgen und weitere historische Quartiere

### Stadtteile und Randgebiete
Die Gebiete werden anhand der offiziellen Distrikt-/Verkehrszellenkarten abgeglichen. Zu berücksichtigen sind unter anderem:
- City, Altstadt, Kreuz, Birken, Saas und Moritzhöfen
- Meyernberg, Roter Hügel, Oberobsang und Herzoghöhe
- Hammerstatt, St. Georgen, Laineck und St. Johannis
- Aichig, Seulbitz, Wolfsbach und Oberkonnersreuth
- Destuben, Thiergarten, Dörnhof, Oberpreuschwitz und weitere Randlagen
- Unterpreuschwitz als ausdrücklich gewünschter eigener Prüf- und Modellierungsbereich

Die genaue Zuordnung und Schreibweise jedes Orts wird vor dem Bau gegen offizielle Karten und Kartendaten geprüft. Benachbarte Orte außerhalb der Stadtgrenze werden nicht stillschweigend als Stadtgebiet ausgegeben; sie können als angrenzende Spielwelt-Erweiterung aufgenommen werden.

## Wiedererkennbare Sehenswürdigkeiten

Jeder wichtige Ort erhält:
1. korrekte Position und Ausrichtung im Straßennetz;
2. eine charakteristische Silhouette und Dach-/Fassadenform;
3. passende Umgebung (Platz, Wege, Bäume, Parkplätze, Wasser);
4. ein eigenes Namensschild oder Kartenlabel im Spiel.

Priorität für erste individuelle Modelle:
- Marktplatz / Innenstadt-Ensemble
- Rotmain-Center
- Festspielhaus
- Hofgarten
- Hauptbahnhof
- Eremitage

Ein Modell gilt erst als fertig, wenn es im Spiel anhand von Form und Umgebung erkennbar ist – nicht nur, weil ein Schild den Namen zeigt.

## Daten- und Lizenzregeln

- OpenStreetMap-Daten dürfen nur unter Beachtung der ODbL und mit sichtbarer, korrekter Quellenangabe verwendet werden.
- Offizielle Stadtpläne, Orthofotos und 3D-Schrägbilder dienen zunächst zur Recherche. Bildmaterial oder 3D-Daten werden nicht ungeprüft ins Spiel kopiert; Nutzungsrechte müssen separat geklärt sein.
- Es werden keine privaten Grundstücksdetails oder personenbezogenen Informationen ergänzt.
- Fehlende oder widersprüchliche Daten werden als offene Punkte markiert statt erfunden.

## Technische Umsetzung auf Nintendo 3DS

- Offline-Datenpakete; kein Live-Kartenstreaming.
- Stadt in kleine Weltkacheln teilen.
- Straßen und Gebäudeumrisse zunächst stark vereinfachen; nahe Gebäude mit mehr Details.
- Individuelle Modelle nur für wichtige Wahrzeichen und ausgewählte Gebäude.
- Sichtweite, Speicherverbrauch und Bildrate auf echter 3DS-Hardware prüfen.
- Kacheln mit Nachbargebieten sauber verbinden, damit die Stadt nicht aus isolierten Mini-Leveln besteht.

## Umsetzungsreihenfolge

1. Kartengrundlage und Gebietsindex aufbauen.
2. Innenstadt-Straßenlayout und Gelände in der Engine sichtbar machen.
3. Marktplatz und Rotmain-Center als erste erkennbare Landmarken modellieren.
4. Spieler, Kamera, Kollisionen und Orientierung stabilisieren.
5. Weitere Innenstadtbereiche und Wahrzeichen ergänzen.
6. Stadtteile und Dörfer systematisch kachelweise ergänzen – ausdrücklich einschließlich Unterpreuschwitz.
7. Mit jedem neuen Gebiet eine spielbare CIA bauen und auf echter Hardware testen.

## Qualitätskontrolle pro Gebiet

- Straßen und Kreuzungen stimmen in ihrer Lage grob mit der Vorlage überein.
- Gebäude stehen an den richtigen Stellen und haben plausible Grundrisse.
- Markante Gebäude sind durch ihre Form erkennbar.
- Übergänge zu Nachbarkacheln passen.
- Keine leeren Flächen oder bloß generischen Häuserblöcke, wo ein markanter Ort erwartet wird.
- Laufzeit, Speicher und Bildrate bleiben auf dem 3DS stabil.

## Quellen für die Planung

- Stadt Bayreuth – Stadtgebiet und Flächen-/Straßennetzangaben: https://www.bayreuth.de/rathaus-buergerservice/stadtverwaltung/zahlen-fakten-2/stadtgebiet/
- Stadt Bayreuth – Stadtdistriktplan (PDF): https://www.bayreuth.de/wp-content/uploads/2016/10/Stadtplan-Stadtdistrikte-Stand-2016.pdf
- Nahverkehrsplan Bayreuth – Gebietskategorien und 54 Verkehrszellen: https://www.bayreuth.de/wp-content/uploads/2025/02/NVP_Stadt_BT.pdf
- Stadt Bayreuth – Stadtplan, 3D-Viewer und Geoportal: https://www.bayreuth.de/rathaus-buergerservice/online-service/
- OpenStreetMap: https://www.openstreetmap.org/ (ODbL beachten)
