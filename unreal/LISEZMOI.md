# Dernier Rempart sous Unreal Engine 5

Ce dossier contient la version **Unreal Engine 5** du jeu, refaite petit à petit pendant les séances programmées (lundi, mercredi et vendredi). La version Godot, à la racine du dépôt, reste jouable telle quelle pendant ce temps.

Chaque séance ajoute une étape. Unreal ne peut pas tourner sur le serveur de Claude : c'est toi qui compiles et testes chaque étape sur ton PC. Si quelque chose ne marche pas, envoie le message d'erreur (le texte ou une capture d'écran) dans le fil « Travail régulier sur le jeu », et la séance suivante le corrige en premier.

## État du portage

| Étape | Contenu | État |
|---|---|---|
| 1 | Projet de départ : personnage à la première personne, ZQSD, Lumen, ombres virtuelles, terrain d'essai | à tester |
| 2 | Import des textures et des modèles (armes, zombies) | à faire |
| 3 | Carte du chapitre 1 | à faire |
| 4 | Tir, pistolet et fusil | à faire |
| 5 | Zombies sur le chemin, PV et barres de vie | à faire |
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

1. Double-clique sur **DernierRempart.uproject**.
2. Unreal annonce que des modules manquent (« The following modules are missing or built with a different engine version: DernierRempart. Would you like to rebuild them now? ») : clique sur **Oui** (Yes). La compilation prend quelques minutes, sans fenêtre visible.
3. L'éditeur s'ouvre. **La première fois, il prépare ses shaders** (« Compiling Shaders » en bas à droite) : ça peut durer de 10 à 40 minutes. Laisse-le finir.

Si Windows ne sait pas avec quoi ouvrir le fichier, ou si tu as installé une autre version qu'Unreal 5.8 : clic droit sur **DernierRempart.uproject** (sous Windows 11 : « Afficher d'autres options »), puis **Switch Unreal Engine version...**, et choisis ta version.

## 6. Tester l'étape 1

1. Au milieu de l'éditeur, la scène est vide ou noire : c'est normal. Le terrain d'essai se construit quand on lance la partie.
2. Clique sur le bouton vert **▶ Play** en haut (ou appuie sur **Alt + P**), puis clique dans l'image pour que le jeu prenne la souris.
3. Tu dois voir : un grand sol brun sous un ciel bleu avec le soleil, un mur de béton devant toi, un mur rouge sur la gauche, trois caisses, un petit toit sur quatre piliers, des ombres douces, et un léger voile au loin. Près du mur rouge, le sol prend une teinte rosée : c'est la lumière qui rebondit (Lumen). Un message en haut à gauche rappelle les touches.
4. Essaie : **ZQSD** pour bouger, la **souris** pour regarder, **Espace** pour sauter, **Maj** pour courir (en avançant). Le personnage doit marcher, courir et sauter comme dans la version Godot.
5. **Échap** arrête la partie et te ramène à l'éditeur.

Dis-moi aussi si la souris te paraît trop lente ou trop rapide par rapport à la version Godot : sa vitesse est calculée, pas encore mesurée.

## 7. Si ça ne marche pas

- **« DernierRempart could not be compiled. Try rebuilding from source manually. »** :
  1. Clic droit sur **DernierRempart.uproject** (sous Windows 11 : « Afficher d'autres options »), puis **Generate Visual Studio project files**.
  2. Ouvre le fichier **DernierRempart.sln** qui vient d'apparaître (avec Visual Studio).
  3. Menu **Générer**, puis **Générer la solution**.
  4. Ouvre la **Liste d'erreurs** (menu Affichage) et copie les premières lignes rouges dans le fil.
- **Un autre message** : fais une capture d'écran de la fenêtre et envoie-la dans le fil.

## Bon à savoir

- Les dossiers `Binaries`, `Intermediate`, `Saved` et `DerivedDataCache` sont créés par Unreal sur ton PC. Ils ne partent pas sur GitHub (ils sont ignorés), donc GitHub Desktop ne te les proposera pas.
- La version Godot n'est pas touchée : tu peux continuer à y jouer comme avant.
