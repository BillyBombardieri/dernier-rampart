# Dernier Rempart

FPS + tower defense solo. Tu défends une base contre des vagues de zombies : tu poses des tours sur des ancrages et tu te bats toi-même en vue à la première personne.

Ce dépôt contient le **prototype jouable**. Il sert à vérifier que le mélange FPS + tours est amusant avant d'ajouter du contenu.

Côté ambiance, la partie se joue au crépuscule, dans le brouillard, avec des projecteurs et des barils en feu. Les textures sont réalistes (terre, boue, béton, métal rouillé). Les zombies sont de **vrais modèles 3D** sculptés, texturés et animés : vêtements déchirés, plaies, démarche boiteuse, course, coups de griffes, crachat, cri, chute en arrière ou face contre terre. Le pistolet et le fusil sont modélisés pièce par pièce : la glissière et la culasse reculent à chaque tir, l'arme pivote au rechargement et on voit le chargeur sortir puis rentrer. En visée, tu vises avec les vrais organes de visée : les trois points lumineux du pistolet, le réticule rouge du viseur holographique du fusil. Les armes ont du recul, un flash et des sons, et tu as une lampe torche.

## Récupérer le jeu sur Windows (et le mettre à jour facilement)

La méthode la plus simple est **GitHub Desktop**. Il suffit d'un clic pour récupérer chaque mise à jour.

1. Installe **GitHub Desktop** : https://desktop.github.com puis connecte-toi avec ton compte GitHub.
2. Va dans **File → Clone repository**, onglet **GitHub.com**, choisis `BillyBombardieri/dernier-rampart`, puis clique sur **Clone**. Le dossier est créé par défaut dans `Documents\GitHub\dernier-rampart`.
3. Pour récupérer une mise à jour : ouvre GitHub Desktop, clique sur **Fetch origin**, puis sur **Pull origin** s'il apparaît. C'est tout.

Si tu as modifié des fichiers de ton côté, GitHub Desktop te le signale avant de mettre à jour.

## Lancer le jeu

1. Installe **Godot 4.7** (version standard, pas .NET) : https://godotengine.org/download
2. Dans Godot, clique sur **Importer**, choisis le fichier `project.godot` du dossier `dernier-rampart`, puis sur **Importer et modifier**.
3. Appuie sur **F5** (ou le bouton ▶ en haut à droite).

Après une mise à jour avec GitHub Desktop, Godot recharge les fichiers tout seul. Si une fenêtre te demande de recharger, clique sur **Recharger**.

## Menu principal et réglages

Le jeu s'ouvre sur le **menu principal** : **JOUER** (choix du niveau), **AMÉLIORATIONS** (améliorations permanentes), **TUTORIEL** (rejoue le tutoriel guidé), **RÉGLAGES** et **QUITTER**. Ton **record** (meilleur score et vague atteinte), les niveaux débloqués et tes insignes s'affichent en dessous.

Dans les **réglages** (aussi accessibles en partie avec **Échap**) :

- **Sensibilité de la souris**, **volume général**, **champ de vision** et **inverser l'axe vertical** ;
- **Touches** : clique sur une action, puis appuie sur la nouvelle touche ou le bouton de souris (Échap pour annuler). Si la touche sert déjà à une autre action, les deux actions échangent leurs touches. **Touches par défaut** remet tout comme au départ.

Tout est sauvegardé dans `reglages.cfg`, dans le dossier de sauvegarde de Godot (`%APPDATA%\Godot\app_userdata\Dernier Rempart` sous Windows) : réglages, touches, tutoriel déjà fait, record, niveaux débloqués, insignes et améliorations.

## Les 4 niveaux

Chaque niveau a sa propre carte (chemin, barrières, ancrages, décor) et sa propre ambiance. Ils se débloquent l'un après l'autre : **gagner un niveau ouvre le suivant**. Chaque niveau est plus dur que le précédent : plus de zombies, plus solides, qui frappent plus fort, et les zombies spéciaux arrivent plus tôt. Les zombies ne vont **jamais plus vite** d'un niveau à l'autre.

| Niveau | Environnement | Difficulté |
|---|---|---|
| 1 · La Brèche | Terrain vague au crépuscule, voitures rouillées, arbres morts | Normale |
| 2 · Le Marais | Marécage noyé de brume, mares noires, roseaux, lucioles | PV x1,15, dégâts x1,1, +15 % de zombies, spéciaux 1 vague plus tôt |
| 3 · La Raffinerie | Zone industrielle, conteneurs, cuves, projecteurs au sodium, cendres | PV x1,3, dégâts x1,2, +30 % de zombies, spéciaux 1 vague plus tôt, 2 boss à la vague 10 |
| 4 · Le Col Gelé | Village de montagne en ruine, sapins, blizzard | PV x1,5, dégâts x1,3, +45 % de zombies, spéciaux 2 vagues plus tôt, 2 boss à la vague 10 |

Après une victoire, le bouton **NIVEAU SUIVANT** lance directement le niveau débloqué.

## Améliorations permanentes

Chaque partie rapporte des **insignes ★** : 1 par vague repoussée (x1,5 au niveau 2, x2 au niveau 3, x3 au niveau 4), plus une prime en cas de victoire (5 x le numéro du niveau). Tu les dépenses dans **AMÉLIORATIONS**, au menu principal. Elles restent d'une partie à l'autre. Les bonus sont modestes, pour que le jeu reste un défi :

| Amélioration | Par rang | Rangs | Prix |
|---|---|---|---|
| Vitalité | +6 % de points de vie | 3 | 3, 6, 10 |
| Armurier | +5 % de dégâts aux armes | 3 | 4, 8, 12 |
| Ingénieur | +5 % de dégâts des tours | 3 | 4, 8, 12 |
| Maçonnerie | +8 % de PV aux barrières | 3 | 3, 6, 10 |
| Réserves | +10 ferraille au départ | 3 | 2, 5, 8 |
| Récupération | +10 % de prime de vague | 3 | 3, 6, 9 |
| Dynamo | +1 énergie pour les tours | 2 | 6, 12 |

**Tout rembourser** rend tous les insignes dépensés, pour essayer une autre combinaison.

## Premier lancement : le tutoriel

À la première partie, un **tutoriel guidé** (15 étapes, en haut à droite) t'apprend à te déplacer, repérer le portail, comprendre les barrières, poser et choisir des tours, gérer l'énergie, utiliser l'établi, tirer, marquer, lancer un gadget, faire les combos et ramasser la ferraille. Le compte à rebours de la première vague est en pause tant que tu ne la lances pas avec **Entrée**. Appuie sur **P** pour passer le tutoriel. Pour le revoir, clique sur **TUTORIEL** dans le menu principal.

## Interface

- **En haut à gauche** : la minimap ronde, qui tourne avec toi. Elle montre le chemin, les barrières (en rouge pointillé si elles sont détruites), les ancrages, les tours, l'établi, le Cœur, les zombies (couleur selon le type), la ferraille et le portail (**!** rouge, collé au bord s'il est loin). En dessous : ferraille, énergie et **score**.
- **En haut au centre** : la vague, la phase, le temps restant ou le nombre de zombies restants, la vie du Cœur et celle des **deux barrières** (une barrière détruite affiche où en est sa remise en place).
- **En haut à droite** : l'objectif du tutoriel.
- **En bas à gauche** : ta vie, tes implants et tes **deux gadgets** (prêts ou en recharge).
- **En bas à droite** : l'arme, les munitions, le type de munitions spéciales et le rechargement.
- **Au centre** : un viseur qui s'écarte quand tu bouges ou tires, et une croix quand tu touches (rouge si le zombie meurt). Il disparaît quand tu vises : ce sont alors les organes de visée de l'arme qui comptent.
- **Barre de construction** : quand tu regardes un ancrage vide, les 6 tours s'affichent avec leur prix, leur énergie et leur rôle. Celle qui est en surbrillance sera construite.
- **Flèches au bord de l'écran** : la direction et la distance du **Cœur** (bleu), et du **portail** (rouge) au début de la partie.
- **H** affiche toute l'aide des commandes, avec les touches que tu as choisies.

Le **portail** des zombies est signalé par une fine colonne de lumière rouge, un panneau et une flèche au bord de l'écran, **seulement au début de la partie** (avant la première vague). Ensuite, il reste discret : une lueur rouge au sol, un peu de fumée, et le **!** sur la minimap.

## Commandes (par défaut, clavier AZERTY)

Toutes les touches se changent dans **Réglages → Touches**.

| Touche | Action |
|---|---|
| ZQSD | Se déplacer |
| Souris | Regarder |
| Clic gauche | Tirer |
| Clic droit (maintenu) | Viser (plus précis, zoom) |
| & / é (ou 1 / 2 du pavé numérique, ou molette) | Pistolet lourd / Fusil d'assaut |
| R | Recharger |
| Espace / Maj | Sauter / Courir |
| F | Marquer un zombie : les tours le ciblent en priorité (+25 % de dégâts) |
| Molette ou C sur un ancrage vide | Choisir la tour à construire |
| E sur un ancrage vide | Construire la tour choisie |
| E sur une tour | Améliorer (3 niveaux) |
| X sur une tour | Allumer / éteindre (libère de l'énergie) |
| Maintenir E près d'une barrière ou sur le Cœur | Réparer, ou relever une barrière détruite (1 ferraille pour 10 PV) |
| E sur l'établi (entre les vagues) | Ouvrir l'établi |
| A / G | Gadget 1 / Gadget 2 |
| Entrée | Lancer la vague, passer la récolte |
| L | Allumer / éteindre la lampe torche |
| H | Afficher / masquer l'aide |
| P | Passer le tutoriel |
| Échap | Pause : reprendre, réglages, recommencer, menu principal, quitter |

## Ce que contient le prototype

- **4 niveaux**, chacun sur sa carte : un portail, un chemin, deux avant-postes sur le chemin (Avant-poste et Muraille) et le Cœur.
- **Phases** : Préparation → Assaut → Récolte, sur **10 vagues**. Les vagues 5 et 10 sont des **Nuits de siège** avec un boss.
- **Deux ressources** :
  - la **ferraille**, laissée au sol par les zombies : il faut aller la ramasser, et elle disparaît à la fin de la récolte. Chaque vague repoussée rapporte aussi une prime (10 + 5 x le numéro de la vague) ;
  - l'**énergie** (6 au départ) : chaque tour allumée en consomme 1 à 3.

### Barrières sur le chemin

Chaque avant-poste est une **barrière** en travers du chemin (blocs de béton, grillage, barbelés et barrière levante), entourée d'ancrages pour les tours : **600 PV** pour l'Avant-poste, **900 PV** pour la Muraille. Rien n'est construit en dehors du chemin. Les zombies doivent casser la barrière pour passer, pendant que les tours qui l'encadrent les mitraillent. Ils n'attaquent **jamais les tours** : seulement les barrières, toi et le Cœur.

Pour réparer, **maintiens E** en regardant la barrière ou simplement à côté d'elle (80 PV par seconde, 1 ferraille pour 10 PV). Une fois détruite, elle laisse passer les zombies : maintiens E près des débris pour la relever. Ses éléments se redressent petit à petit et elle bloque de nouveau à **30 % de ses PV** (180 PV pour l'Avant-poste). Sans ferraille, un message te le dit. Le Cracheur crache par-dessus, et le Fouisseur creuse dessous.

### 7 zombies

| Zombie | À partir de | Particularité |
|---|---|---|
| Rôdeur | vague 1 | Le zombie de base |
| Coureur | vague 1 | Rapide, il te prend en chasse |
| Brute | vague 2 | Très solide, frappe fort sur les barrières |
| Cracheur (peau verdâtre, poche lumineuse sous la gorge) | vague 3 | Reste en retrait et crache de l'acide sur la barrière et sur toi, par-dessus la horde |
| Hurleur (mâchoire démesurée) | vague 4 | S'arrête pour hurler, bras levés : son cri rend les zombies proches plus rapides et plus forts (yeux rouges) ; les tours le visent en priorité |
| Fouisseur (torse nu, couvert de terre) | vague 5 | S'enterre devant une barrière et ressort juste derrière. Intouchable sous terre. Un Phare le fait sortir de terre |
| Boss de siège | vagues 5 et 10 | Énorme, insensible à l'étourdissement |

Les zombies marchent plutôt lentement (un Rôdeur avance à 1,8 m/s, un Coureur à 3,9 m/s), et un peu plus vite à chaque vague. Un coup porte quand la main du zombie arrive sur toi : en reculant pendant son élan, tu peux l'esquiver.

### 6 tours

| Tour | Prix | Énergie | Rôle |
|---|---|---|---|
| Mitrailleuse | 25 | 2 | Tir rapide sur une cible |
| Cryo | 30 | 2 | Gèle et ralentit |
| Lance-flammes | 35 | 2 | Cône de feu à courte portée, enflamme |
| Arc électrique | 40 | 3 | L'éclair saute sur plusieurs zombies et les charge |
| Mortier | 45 | 3 | Obus de zone à très longue portée, étourdit (pas de tir à moins de 6 m) |
| Phare | 30 | 1 | Renforce la portée et les dégâts des tours proches, marque un zombie de temps en temps et fait sortir les Fouisseurs de terre |

Chaque tour s'améliore 2 fois (plus de dégâts et de portée). Les zombies ne s'en prennent pas aux tours : elles ne s'abîment jamais.

### 3 combos

- **BRISÉ** : un tir de pistolet lourd sur un zombie gelé (Cryo) fait x3 dégâts.
- **SURCHARGE** : un zombie chargé (Arc électrique ou balles électriques) touché par une de tes balles normales libère une onde électrique qui blesse et étourdit les zombies autour.
- **EMBRASEMENT** : un obus de Mortier sur un zombie en feu (Lance-flammes ou balles incendiaires) propage l'incendie à tous les zombies autour, et les zombies en feu prennent x1,5 dégâts.

Les combos rapportent des points et deviennent plus forts au fil des vagues.

### L'établi

L'**établi** est près du Cœur. Il s'utilise entre les vagues, et le jeu est en pause tant qu'il est ouvert. Tout se paie en ferraille :

- **Canon** : +20 % de dégâts par niveau (2 niveaux), pour chaque arme ;
- **Chargeur** : +30 % de balles par niveau (2 niveaux), pour chaque arme ;
- **Munitions spéciales** (60 chacune, par arme) : **incendiaires** (mettent le feu), **perforantes** (traversent jusqu'à 3 zombies, +25 % sur Brutes et Boss) ou **électriques** (chargent la cible pour la SURCHARGE). Une fois achetées, tu changes de munitions gratuitement ;
- **Gadgets** : choisis les 2 gadgets de tes touches A et G ;
- **Générateur** : +2 énergie par niveau (3 niveaux).

### 3 gadgets

| Gadget | Recharge | Effet |
|---|---|---|
| Barricade | 35 s | Barre le chemin devant toi pendant 20 s (250 PV). À poser sur le chemin |
| Leurre sonore | 30 s | Se lance, bipe et attire les zombies proches pendant 8 s |
| Drone de récolte | 45 s | Ramasse la ferraille autour de toi pendant 25 s |

### Le reste

- **Santé** : elle remonte au maximum dès qu'une vague est repoussée.
- **Implants** : après un siège, tu choisis 1 implant parmi 3. Chacun a un bonus et un malus.
- **Barre de vie** : une fine barre apparaît au-dessus d'un zombie blessé (du vert au rouge), puis s'efface s'il n'est plus touché.
- **Tir à la tête** : x2 dégâts. La zone de la tête suit l'animation (un zombie penché a la tête plus bas).
- **Score** : chaque zombie abattu rapporte des points (10 pour un Rôdeur, 300 pour un Boss), chaque combo +10, chaque vague repoussée 50 x son numéro, et la victoire 1000 plus les PV restants du Cœur. Le record est gardé.
- **Mort** : tu réapparais au Cœur après un délai qui s'allonge à chaque mort. La partie est perdue si le Cœur tombe.

## Organisation du code

Tout est en GDScript dans `scripts/`. La carte est construite par le code, sans scène à éditer à la main. Le jeu démarre sur `scenes/menu.tscn`, qui lance `scenes/main.tscn`.

| Fichier | Rôle |
|---|---|
| `game.gd` | État global (autoload `Game`) : ressources, score, implants, améliorations de l'établi, gadgets |
| `settings.gd` | Réglages (autoload `Settings`) : sensibilité, volume, touches, tutoriel, record |
| `menu.gd` | Menu principal |
| `level_panel.gd`, `upgrade_panel.gd` | Fenêtres du choix du niveau et des améliorations permanentes |
| `levels.gd` | Les 4 niveaux : chemin, barrières, ancrages, ambiance, décor, météo et difficulté |
| `upgrades.gd` | Améliorations permanentes et calcul des insignes |
| `main.gd` | Construction de la carte du niveau : ambiance, chemin, avant-postes (barrières et ancrages), établi, Cœur, décor et météo |
| `wave_manager.gd` | Phases et composition des vagues |
| `player.gd` | Joueur FPS : tir, munitions spéciales, marquage, construction, réparation |
| `tower.gd`, `socket.gd` | Les 6 tours et leurs ancrages |
| `zombie.gd` | Types de zombies, déplacement, animations, états (gelé, en feu, chargé, étourdi, enragé, enterré) |
| `health_bar.gd` | Barre de vie au-dessus des zombies blessés |
| `zombie_pose.gd` | Réactions ajoutées à l'animation : recul quand un zombie est touché, vacillement quand il est étourdi |
| `zombie_models.gd` | Repères des modèles (yeux, vitesse de marche des animations), écrits par `tools/make_models.py` |
| `barrier.gd` | Barrières du chemin et barricade |
| `projectile.gd` | Crachat d'acide et obus de mortier |
| `gadgets.gd`, `decoy.gd`, `drone.gd` | Gadgets : barricade, leurre sonore, drone de récolte |
| `workbench.gd`, `workbench_panel.gd` | L'établi et sa fenêtre |
| `structure.gd` | Le Cœur (bâtiment avec des PV) |
| `scrap.gd` | Ferraille à ramasser |
| `hud.gd` | Interface (panneaux, viseur, barre de construction, notifications, fin de partie) |
| `minimap.gd` | Minimap ronde |
| `pause_menu.gd`, `settings_panel.gd` | Menu pause et fenêtre des réglages |
| `tutorial.gd` | Tutoriel guidé |
| `ui.gd` | Boutons, panneaux, curseurs et transition d'ouverture communs aux menus |
| `fx.gd` | Matériaux réalistes, particules, flashs, éclairs, traînées de tir, textes flottants |
| `sfx.gd` | Sons (dans l'espace 3D ou à plat) |
| `flicker.gd` | Lumières qui vacillent (feu, lampe de secours) |

## Textures et sons

Toutes les textures (`assets/textures`) et tous les sons (`assets/sounds`) sont générés par le script `tools/generate_assets.py`, à partir de bruit mathématique. Aucune ressource externe n'est utilisée, donc il n'y a aucun problème de droits. Pour les régénérer : `python tools/generate_assets.py` (il faut numpy et pillow).

## Modèles 3D (zombies et armes)

Les modèles de `assets/models` sont fabriqués par le code avec **Blender 4** (gratuit), sans aucune ressource externe :

- `tools/sdf.py` : un petit moteur de sculpture (volumes qui se fondent les uns dans les autres, transformés en maillage) ;
- `tools/sculpt.py` : l'anatomie de chaque zombie (muscles, côtes, visage creusé), ses vêtements plissés et déchirés, ses plaies et ses couleurs ;
- `tools/zombie_anims.py` : le squelette et les animations (marche, course, attaque, crachat, cri, creusage, deux morts) ;
- `tools/weapon_models.py` : le pistolet et le fusil, pièce par pièce ;
- `tools/make_models.py` : l'enchaînement complet. Chaque zombie est sculpté en détail, puis allégé pour le jeu (8 000 triangles, 11 000 pour le boss) ; les couleurs, le relief fin et les ombres sont « cuits » dans des textures.

Pour tout régénérer, depuis le dossier du dépôt (compter une dizaine de minutes) :

```
blender -b --factory-startup -P tools/make_models.py
```

Ajoute `-- --only rodeur,pistolet` pour ne refaire que certains modèles, et `--preview <dossier>` pour obtenir des images de contrôle. Le Python intégré à Blender doit avoir numpy (c'est le cas de la version Windows de blender.org).

## Tests automatiques

Une partie simulée de 10 vagues (tours posées partout, joueur immobile, Cœur presque indestructible). Ajoute `-- 2` (ou 3, 4) pour la jouer sur un autre niveau :

```
godot --headless --fixed-fps 60 --path . res://tests/smoke_test.tscn
```

Les fonctionnalités une par une (barres de vie, barrières et réparation en maintenant E, combos, zombies, tours, Phare, santé entre les vagues, établi, gadgets, touches, score, menus, améliorations permanentes, les 4 niveaux) :

```
godot --headless --fixed-fps 60 --path . res://tests/feature_test.tscn
```

Le chargement de tous les scripts et scènes :

```
godot --headless --path . res://tests/load_check.tscn
```

Des captures d'écran (menu, barrière attaquée, les sept zombies, visée au fusil, rechargement, zombies de près, construction, établi, réglages) :

```
godot --path . --fixed-fps 60 res://tests/screenshot.tscn -- <dossier_de_sortie>
```

Le concept complet du jeu est dans le document de conception du projet.
