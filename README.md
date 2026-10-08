# Dernier Rempart

FPS + tower defense solo. Tu défends une base contre des vagues de zombies : tu poses des tours sur des ancrages et tu te bats toi-même en vue à la première personne.

Ce dépôt contient le **premier prototype jouable**. Il sert à vérifier que le mélange FPS + tours est amusant avant d'ajouter du contenu.

Côté ambiance, la partie se joue au crépuscule, dans le brouillard, avec des projecteurs et des barils en feu. Les textures sont réalistes (terre, boue, béton, métal rouillé). Les zombies ont une forme humaine animée : ils marchent, frappent, encaissent les tirs et tombent au sol. Les armes ont du recul, un flash, une visée et des sons, et tu as une lampe torche.

## Récupérer le jeu sur Windows (et le mettre à jour facilement)

La méthode la plus simple est **GitHub Desktop**. Il suffit d'un clic pour récupérer chaque mise à jour.

1. Installe **GitHub Desktop** : https://desktop.github.com puis connecte-toi avec ton compte GitHub.
2. Va dans **File → Clone repository**, onglet **GitHub.com**, choisis `snkrsbilly-wq/dernier-rampart`, puis clique sur **Clone**. Le dossier est créé par défaut dans `Documents\GitHub\dernier-rampart`.
3. Pour récupérer une mise à jour : ouvre GitHub Desktop, clique sur **Fetch origin**, puis sur **Pull origin** s'il apparaît. C'est tout.

Si tu as modifié des fichiers de ton côté, GitHub Desktop te le signale avant de mettre à jour.

## Lancer le jeu

1. Installe **Godot 4.7** (version standard, pas .NET) : https://godotengine.org/download
2. Dans Godot, clique sur **Importer**, choisis le fichier `project.godot` du dossier `dernier-rampart`, puis sur **Importer et modifier**.
3. Appuie sur **F5** (ou le bouton ▶ en haut à droite).

Après une mise à jour avec GitHub Desktop, Godot recharge les fichiers tout seul. Si une fenêtre te demande de recharger, clique sur **Recharger**.

## Premier lancement : le tutoriel

À la première partie, un **tutoriel guidé** (12 étapes, en haut à droite) t'apprend à te déplacer, repérer le portail, poser des tours, gérer l'énergie, tirer, marquer, faire le combo BRISÉ et ramasser la ferraille. Le compte à rebours de la première vague est en pause tant que tu ne la lances pas avec **Entrée**. Appuie sur **P** pour passer le tutoriel. Il ne se relance plus ensuite. Pour le revoir, supprime le fichier `reglages.cfg` du dossier de sauvegarde de Godot (`%APPDATA%\Godot\app_userdata\Dernier Rempart` sous Windows).

## Interface

- **En haut à gauche** : la minimap ronde, qui tourne avec toi. Elle montre le couloir, les ancrages, les tours, les relais, le Cœur, les zombies (points rouges), la ferraille et le portail (**!** rouge, collé au bord s'il est loin). En dessous : ferraille et énergie.
- **En haut au centre** : la vague, la phase, le temps restant ou le nombre de zombies restants, et la vie du Cœur.
- **En haut à droite** : l'objectif du tutoriel.
- **En bas à gauche** : ta vie et tes implants.
- **En bas à droite** : l'arme, les munitions et le rechargement.
- **Au centre** : un viseur qui s'écarte quand tu bouges ou tires, et une croix quand tu touches (rouge si le zombie meurt).
- **Flèches au bord de l'écran** : la direction et la distance du **portail** (rouge) et du **Cœur** (bleu).
- **H** affiche toute l'aide des commandes.

Le **portail** des zombies se repère de partout : une colonne de lumière rouge monte dans le ciel, avec de la fumée rouge et un panneau. La colonne pulse plus fort pendant un assaut.

## Commandes

| Touche | Action |
|---|---|
| ZQSD | Se déplacer (clavier AZERTY) |
| Souris | Viser |
| Clic gauche | Tirer |
| Clic droit (maintenu) | Viser (plus précis, zoom) |
| L | Allumer / éteindre la lampe torche |
| & / é (ou 1 / 2) | Pistolet lourd / Fusil d'assaut |
| R | Recharger |
| Espace / Maj | Sauter / Courir |
| F | Marquer un zombie : les tours le ciblent en priorité (+25 % de dégâts) |
| E sur un ancrage vide | Poser une Mitrailleuse |
| C sur un ancrage vide | Poser une Cryo |
| E sur une tour | Améliorer (3 niveaux) |
| X sur une tour | Allumer / éteindre (libère de l'énergie) |
| Maintenir E sur un relais ou le Cœur | Réparer (coûte de la ferraille) |
| Entrée | Passer le temps de préparation ou de récolte |
| H | Afficher / masquer l'aide |
| P | Passer le tutoriel |
| Échap | Libérer la souris |

## Ce que contient le prototype

- **1 carte** : un portail, un couloir, deux anneaux (Avant-poste et Muraille) et le Cœur.
- **Phases** : Préparation → Assaut → Récolte, sur **10 vagues**. Les vagues 5 et 10 sont des **Nuits de siège** avec un boss.
- **Deux ressources** :
  - la **ferraille**, laissée au sol par les zombies : il faut aller la ramasser, et elle disparaît à la fin de la récolte ;
  - l'**énergie** (6 au départ) : chaque tour allumée en consomme 2.
- **2 tours** : Mitrailleuse et Cryo (la Cryo ralentit et gèle).
- **Combo BRISÉ** : un tir de pistolet lourd sur un zombie gelé fait x3 dégâts.
- **Relais** : si le relais d'un anneau est détruit, les tours de cet anneau s'éteignent jusqu'à ce que tu le répares.
- **3 zombies + 1 boss** :
  - le Rôdeur ;
  - le Coureur, qui te prend en chasse ;
  - la Brute, qui casse les tours et les relais ;
  - le Boss de siège.
- **Implants** : après un siège, tu choisis 1 implant parmi 3. Chacun a un bonus et un malus.
- **Tir à la tête** : x2 dégâts.
- **Mort** : tu réapparais au Cœur après un délai qui s'allonge à chaque mort. La partie est perdue si le Cœur tombe.

## Organisation du code

Tout est en GDScript dans `scripts/`. La carte est construite par le code, sans scène à éditer à la main.

| Fichier | Rôle |
|---|---|
| `game.gd` | État global (autoload `Game`) : ressources, implants, commandes |
| `main.gd` | Construction de la carte, du couloir, des anneaux et du Cœur |
| `wave_manager.gd` | Phases et composition des vagues |
| `player.gd` | Joueur FPS : tir, marquage, construction, réparation |
| `tower.gd`, `socket.gd` | Tours et ancrages |
| `zombie.gd` | Types de zombies, déplacement, états (gelé, marqué) |
| `structure.gd` | Cœur et relais (bâtiments avec des PV) |
| `scrap.gd` | Ferraille à ramasser |
| `hud.gd` | Interface (panneaux, viseur, flèches du portail et du Cœur, notifications) |
| `minimap.gd` | Minimap ronde |
| `tutorial.gd` | Tutoriel guidé du premier lancement |
| `fx.gd` | Matériaux réalistes, particules, flashs, traînées de tir, textes flottants |
| `sfx.gd` | Sons (dans l'espace 3D ou à plat) |
| `flicker.gd` | Lumières qui vacillent (feu, lampe de secours) |

## Textures et sons

Toutes les textures (`assets/textures`) et tous les sons (`assets/sounds`) sont générés par le script `tools/generate_assets.py`, à partir de bruit mathématique. Aucune ressource externe n'est utilisée, donc il n'y a aucun problème de droits. Pour les régénérer : `python tools/generate_assets.py` (il faut numpy et pillow).

## Test automatique

Une partie simulée (tours posées partout, joueur immobile) :

```
godot --headless --fixed-fps 60 --path . res://tests/smoke_test.tscn
```

Des captures d'écran de quelques points de vue :

```
godot --path . res://tests/screenshot.tscn -- <dossier_de_sortie>
```

Le concept complet du jeu est dans le document de conception du projet.
