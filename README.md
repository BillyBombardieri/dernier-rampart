# Dernier Rempart

FPS + tower defense solo. Tu défends une base contre des vagues de zombies : tu poses des tours sur des ancrages et tu te bats toi-même en vue à la première personne.

Ce dépôt contient le **premier prototype jouable**. Il sert à vérifier que le mélange FPS + tours est amusant avant d'ajouter du contenu. Les graphismes sont volontairement simples (formes de base).

## Lancer le jeu

1. Installe **Godot 4.3** (version standard, pas .NET) : https://godotengine.org/download
2. Ouvre Godot, clique sur **Importer** et choisis le fichier `project.godot` de ce dossier.
3. Appuie sur **F5** (ou le bouton ▶ en haut à droite).

## Commandes

| Touche | Action |
|---|---|
| ZQSD | Se déplacer (WASD sur un clavier QWERTY) |
| Souris | Viser |
| Clic gauche | Tirer |
| 1 / 2 | Pistolet lourd / Fusil d'assaut |
| R | Recharger |
| Espace / Maj | Sauter / Courir |
| F | Marquer un zombie : les tours le ciblent en priorité (+25 % de dégâts) |
| E sur un ancrage vide | Poser une Mitrailleuse |
| C sur un ancrage vide | Poser une Cryo |
| E sur une tour | Améliorer (3 niveaux) |
| X sur une tour | Allumer / éteindre (libère de l'énergie) |
| Maintenir E sur un relais ou le Cœur | Réparer (coûte de la ferraille) |
| Entrée | Passer le temps de préparation ou de récolte |
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
| `hud.gd` | Interface |
| `fx.gd` | Formes, traînées de tir, textes flottants |

## Test automatique

Une partie simulée (tours posées partout, joueur immobile) :

```
godot --headless --fixed-fps 60 --path . res://tests/smoke_test.tscn
```

Le concept complet du jeu est dans le document de conception du projet.
