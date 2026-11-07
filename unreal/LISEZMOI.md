# Dernier Rempart sous Unreal Engine 5

Ce dossier contient la version **Unreal Engine 5** du jeu, refaite petit à petit pendant les séances programmées (le dimanche soir, quand tu es d'accord). La version Godot, à la racine du dépôt, reste jouable telle quelle pendant ce temps.

Chaque séance ajoute une étape. Unreal ne peut pas tourner sur le serveur de Claude : c'est toi qui compiles et testes chaque étape sur ton PC. Si quelque chose ne marche pas, envoie le message d'erreur (le texte ou une capture d'écran) dans le fil « Travail régulier sur le jeu », et la séance suivante le corrige en premier. Teste les étapes dans l'ordre : chacune s'appuie sur la précédente.

## État du portage

| Étape | Contenu | État |
|---|---|---|
| 1 | Projet de départ : personnage à la première personne, ZQSD, Lumen, ombres virtuelles, terrain d'essai | testé, ça marche (28 oct.) |
| 2 | Import des textures, des modèles (armes, zombies) et des sons | à tester (8 nov.) |
| 3 | Carte du chapitre 1 | à tester (8 nov.) |
| 4 | Tir, pistolet et fusil | à tester (8 nov.) |
| 5 | Zombies sur le chemin, barrières et Cœur avec PV, vie du joueur, barres de vie | à tester (8 nov.) |
| 6 | Vagues, ferraille, énergie | à faire |
| 7 | Tours, marquage, combos | à faire |
| 8 | Interface, menu, réglages et touches modifiables | à faire |
| ensuite | Autres chapitres, gadgets, établi, implants, améliorations, sauvegarde, tutoriel, sons, export Windows | à faire |

## 1. Ce qu'il faut sur le PC

- Windows 10 ou 11 en 64 bits.
- Une carte graphique compatible DirectX 12, avec 6 à 8 Go de mémoire vidéo de préférence.
- 16 Go de mémoire au minimum, 32 Go conseillés.
- Environ 150 Go libres, sur un SSD si possible : le moteur, Visual Studio, et les fichiers qu'Unreal crée la première fois.

## 2. Installer Unreal Engine 5.8

1. Installe l'**Epic Games Launcher** depuis https://www.unrealengine.com/download et connecte-toi avec un compte Epic (gratuit).
2. Dans le Launcher, ouvre **Unreal Engine** (à gauche), puis l'onglet **Bibliothèque**.
3. Clique sur le **+** à côté de « Versions du moteur », choisis **5.8** (la plus récente des 5.8.x), puis **Installer**.
4. Avant de valider, ouvre **Options** : tu peux décocher les plateformes Android, iOS et Linux, et les « symboles de débogage ». Ça économise des dizaines de Go. Garde **Windows**.

## 3. Installer Visual Studio

Unreal a besoin de Visual Studio pour compiler le code C++ du jeu. Tu n'auras pas à écrire de code.

1. Télécharge **Visual Studio Community** (gratuit) : https://visualstudio.microsoft.com/fr/. La version 2026 convient, la 2022 aussi si elle est à jour (17.14 ou plus récente ; si tu l'as déjà, ouvre « Visual Studio Installer » et clique sur **Mettre à jour**).
2. Dans l'installateur, onglet **Charges de travail**, coche :
   - **Développement Desktop en C++**
   - **Développement .NET Desktop**
   - **Développement de jeux en C++**
3. À droite, dans le détail de « Développement de jeux en C++ », vérifie que ces cases sont cochées : **Programme d'installation d'Unreal Engine**, **Outils de profilage C++**, **AddressSanitizer C++**, et un **SDK Windows 11** (10.0.22621 ou plus récent).
4. Clique sur **Installer**, puis redémarre le PC à la fin.

## 4. Récupérer le projet

Dans GitHub Desktop : **Fetch origin**, puis **Pull origin**. Le projet est dans `Documents\GitHub\dernier-rampart\unreal\DernierRempart`.

## 5. Ouvrir le projet

1. Ouvre l'**Epic Games Launcher**, onglet **Unreal Engine**, puis **Bibliothèque**, et clique sur **Lancer** sous ta version 5.8. Dans la fenêtre des projets qui s'ouvre, clique sur **Parcourir** (Browse) et choisis **DernierRempart.uproject**. (Un double-clic sur le fichier marche aussi quand Windows l'ouvre avec Unreal, mais il l'ouvre parfois avec Visual Studio : voir plus bas.)
2. Unreal annonce que des modules manquent (« The following modules are missing or built with a different engine version: DernierRempart. Would you like to rebuild them now? ») : clique sur **Oui** (Yes). La compilation prend quelques minutes, sans fenêtre visible.
3. L'éditeur s'ouvre. **La première fois, il prépare ses shaders** (« Compiling Shaders » en bas à droite) : ça peut durer de 10 à 40 minutes. Laisse-le finir.

Si un double-clic sur le fichier ouvre **Visual Studio** avec le message « Unreal Engine Missing : impossible de trouver un moteur Unreal Engine pour l'uproject sélectionné », c'est que la version 5.8 n'est pas installée dans le Launcher, ou que Windows ouvre le fichier avec Visual Studio au lieu d'Unreal. Vérifie la Bibliothèque du Launcher (installe 5.8 si elle n'y est pas), puis ouvre le projet par le bouton **Lancer** comme à l'étape 1.

Si tu as installé une autre version qu'Unreal 5.8, dis-le dans le fil : le projet sera adapté à ta version.

## 6. Tester l'étape 1

(Depuis l'étape 3, le terrain d'essai est remplacé par la carte du chapitre 1 : passe directement à l'étape 3.)

1. Au milieu de l'éditeur, la scène est vide ou noire : c'est normal. Le terrain d'essai se construit quand on lance la partie.
2. Clique sur le bouton vert **▶ Play** en haut (ou appuie sur **Alt + P**), puis clique dans l'image pour que le jeu prenne la souris.
3. Tu dois voir : un grand sol brun sous un ciel bleu avec le soleil, un mur de béton devant toi, un mur rouge sur la gauche, trois caisses, un petit toit sur quatre piliers, des ombres douces, et un léger voile au loin. Près du mur rouge, le sol prend une teinte rosée : c'est la lumière qui rebondit (Lumen). Un message en haut à gauche rappelle les touches.
4. Essaie : **ZQSD** pour bouger, la **souris** pour regarder, **Espace** pour sauter, **Maj** pour courir (en avançant). Le personnage doit marcher, courir et sauter comme dans la version Godot.
5. **Échap** arrête la partie et te ramène à l'éditeur.

Dis-moi aussi si la souris te paraît trop lente ou trop rapide par rapport à la version Godot : sa vitesse est calculée, pas encore mesurée.

## 7. Tester l'étape 2 : importer les textures et les modèles

Les textures, les modèles et les sons de la version Godot (dossier `assets` du dépôt) doivent être importés une fois dans Unreal. Un script s'en charge.

1. Après **Fetch origin** et **Pull origin** dans GitHub Desktop, ouvre le projet comme d'habitude. Unreal propose de recompiler (« Would you like to rebuild them now? ») : clique sur **Oui**. Il peut aussi annoncer que de nouveaux plugins sont activés (Python) : accepte et laisse-le redémarrer s'il le demande.
2. En bas de l'éditeur, il y a une barre de saisie avec le mot **Cmd** à gauche (si c'est écrit **Python**, clique dessus et remets **Cmd**). Clique dans la barre et tape exactement :

   ```
   py importer_ressources.py
   ```

   et appuie sur **Entrée**. (Autre moyen : menu **Outils** (Tools) > **Exécuter un script Python** (Execute Python Script), puis choisis le fichier `unreal\DernierRempart\Content\Python\importer_ressources.py`.)
3. L'import dure quelques minutes. Ouvre le **Journal de sortie** (Output Log, menu Fenêtre) : la dernière ligne doit être « DernierRempart : import terminé ». S'il y a des lignes rouges, copie-les dans le fil.
4. Dans le **Navigateur de contenu** (Content Drawer, en bas à gauche, ou Ctrl + Espace), tu dois voir un dossier **Importe** avec quatre sous-dossiers : **Textures** (25 textures), **Materiaux** (M_Surface et neuf MI_…), **Modeles** (un dossier par arme et par zombie) et **Sons** (42 sons). Double-clique sur un zombie (l'icône rose, « Skeletal Mesh ») : il doit apparaître avec sa peau et ses vêtements.

Ce dossier **Importe** ne part pas sur GitHub : si un jour il y a un problème, supprime-le et relance le script.

## 8. Tester l'étape 3 : la carte du chapitre 1

1. Fais d'abord l'étape 2 (l'import). Sans elle, la carte s'affiche quand même, mais en couleurs unies, avec un message orange en haut à gauche.
2. Clique sur **▶ Play** (ou **Alt + P**), puis clique dans l'image.
3. Tu dois te retrouver dans « La Brèche », au crépuscule, juste devant le **Cœur**, qui est dans ton dos (le bunker de béton avec sa porte blindée et deux voyants bleus), comme au début d'une partie Godot. En haut à gauche, un message rappelle le nom du chapitre.
4. Regarde autour de toi : le sol de terre, le chemin boueux qui serpente jusqu'à la **brèche** au loin (un mur d'enceinte avec une lampe rouge qui vacille), les deux **barrières** sur le chemin (blocs de béton, poteaux, barrière rayée rouge et blanc), les **dalles rondes** des tours avec leurs repères orangés, les **murs anti-souffle** et les **sacs de sable** autour du Cœur, quatre **projecteurs** sur leurs poteaux, quatre **barils** aux braises rougeoyantes qui éclairent en vacillant, et des rochers, des arbres morts et des carcasses de voitures.
5. Dis-moi ce qui te paraît trop sombre, trop clair ou mal placé : les lumières sont réglées à l'estime (Godot et Unreal ne mesurent pas la lumière de la même façon). Pas encore d'herbe, de flammes ni de grillage : ils viendront plus tard.

## 9. Tester l'étape 4 : le tir

1. Lance la partie (**▶ Play**). L'arme est en bas à droite de l'image, son nom et ses munitions en bas à droite de l'écran, et un viseur au centre.
2. **Clic gauche** tire, **clic droit** vise (l'arme vient devant l'œil, la vue zoome un peu), **R** recharge, **&** (ou **1**) prend le pistolet, **é** (ou **2**) le fusil ; la **molette** change aussi d'arme.
3. Tu dois voir : un éclair au bout du canon, un trait lumineux jusqu'à l'impact, la vue qui monte à chaque tir puis redescend, l'arme qui recule dans la main ; au fusil, garder le bouton enfoncé tire en rafale et la vue monte de plus en plus en dérivant. Tirer la souris vers le bas compense le recul. On entend les coups de feu, le rechargement et ses pas.
4. Le pistolet tire 10 coups, le fusil 30, puis l'arme se recharge toute seule (« Rechargement... »).
5. **Vérifie surtout que l'arme est tenue dans le bon sens**, canon vers l'avant, et dis-moi si ce n'est pas le cas (une capture d'écran suffit) : Unreal range les modèles importés sur d'autres axes que Godot, le code devine le sens d'après la forme de l'arme. Sans l'étape 2, l'arme est remplacée par un simple bloc noir.
6. Les zombies arrivent à l'étape 5, juste en dessous.

## 10. Tester l'étape 5 : les zombies, les barrières et le Cœur

1. Lance la partie. En attendant les vagues (étape 6), un zombie sort du portail toutes les 4 secondes, au plus 10 à la fois : surtout des rôdeurs, parfois un coureur ou une brute.
2. Ils suivent le chemin, s'arrêtent devant la barrière de l'avant-poste et la frappent jusqu'à la casser (elle s'effondre), puis font pareil avec la muraille, puis frappent le Cœur. Ils ne s'en prennent jamais aux tours.
3. En haut au milieu : les PV du Cœur et de chaque barrière. En bas à gauche : ta vie (100 PV).
4. Un zombie touché affiche une fine barre de vie au-dessus de la tête, qui s'efface s'il n'est plus touché pendant 6 s. Un tir dans la tête fait deux fois plus de dégâts.
5. S'ils passent près de toi, ils te poursuivent et te frappent : les bords de l'écran rougissent. À 0 PV, « Tu es tombé ! » s'affiche et tu reviens devant le Cœur après 5 s (3 s de plus à chaque nouvelle chute).
6. **Vérifie surtout que les zombies marchent dans le bon sens et sont bien animés** (marche, course, coups, chute en mourant), et envoie une capture si l'un d'eux glisse, flotte, marche en crabe ou de dos. Sans l'étape 2, ce sont de simples cylindres gris.
7. Il n'y a pas encore de fin de partie : si le Cœur tombe, les zombies restent simplement autour (les vagues et la défaite arrivent à l'étape 6).

## 11. Si ça ne marche pas

- **« DernierRempart could not be compiled. Try rebuilding from source manually. »** :
  1. Clic droit sur **DernierRempart.uproject** (sous Windows 11 : « Afficher d'autres options »), puis **Generate Visual Studio project files**.
  2. Ouvre le fichier **DernierRempart.sln** qui vient d'apparaître (avec Visual Studio).
  3. Menu **Générer**, puis **Générer la solution**.
  4. Ouvre la **Liste d'erreurs** (menu Affichage) et copie les premières lignes rouges dans le fil.
- **Un autre message** : fais une capture d'écran de la fenêtre et envoie-la dans le fil.

## Bon à savoir

- Les dossiers `Binaries`, `Intermediate`, `Saved` et `DerivedDataCache` sont créés par Unreal sur ton PC. Ils ne partent pas sur GitHub (ils sont ignorés), donc GitHub Desktop ne te les proposera pas.
- La version Godot n'est pas touchée : tu peux continuer à y jouer comme avant.
