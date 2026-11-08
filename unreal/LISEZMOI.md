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
| 6 | Vagues, ferraille, points, victoire et défaite | à tester (8 nov.) |
| 7 | Tours, énergie, marquage, combos, réparation des barrières et du Cœur | à tester (8 nov.) |
| 8 | Talents des zombies spéciaux : crachat du Cracheur, cri du Hurleur, terrier du Fouisseur | à tester (8 nov.) |
| 9 | Menu principal, choix du niveau, pause, sauvegarde de la progression | à tester (8 nov.) |
| 10 | Réglages (souris, son, champ de vision, qualité) et touches modifiables | à tester (8 nov.) |
| 11 | Insignes et améliorations permanentes | à tester (8 nov.) |
| 12 | Implants après les Nuits de siège | à tester (8 nov.) |
| 13 | Établi : canon, chargeur, munitions spéciales, générateur | à tester (8 nov.) |
| 14 | Gadgets : barricade, leurre sonore, drone de récolte | à tester (8 nov.) |
| 15 | Décor des chapitres 2 à 6 : mares, conteneurs, cuves, sapins, ruines, immeubles, réverbères, dunes, poteaux | à tester (8 nov.) |
| 16 | Herbe qui ondule au vent et cailloux au sol | à tester (8 nov.) |
| 17 | Météo (neige, pluie, cendres, poussière, lucioles) et vent | à tester (8 nov.) |
| ensuite | Tutoriel, effets de feu et de glace, export Windows | à faire |

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

1. Fais d'abord l'étape 2 (l'import). Sans elle, la carte s'affiche quand même, mais en couleurs unies, avec un message orange en haut à gauche (sous le panneau de la ferraille).
2. Clique sur **▶ Play** (ou **Alt + P**) : depuis l'étape 9, le menu principal s'affiche d'abord. Appuie sur **Entrée** (ou clique sur **JOUER**, puis sur **1**) pour lancer le niveau 1-1, puis clique dans l'image.
3. Tu dois te retrouver dans « La Brèche », au crépuscule, juste devant le **Cœur**, qui est dans ton dos (le bunker de béton avec sa porte blindée et deux voyants bleus), comme au début d'une partie Godot. En haut, un message rappelle le nom du chapitre.
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

1. Lance la partie et appuie sur **Entrée** pour faire venir la première vague tout de suite (voir l'étape 6 juste en dessous).
2. Ils suivent le chemin, s'arrêtent devant la barrière de l'avant-poste et la frappent jusqu'à la casser (elle s'effondre), puis font pareil avec la muraille, puis frappent le Cœur. Ils ne s'en prennent jamais aux tours.
3. En haut au milieu : les PV du Cœur et de chaque barrière. En bas à gauche : ta vie (100 PV).
4. Un zombie touché affiche une fine barre de vie au-dessus de la tête, qui s'efface s'il n'est plus touché pendant 6 s. Un tir dans la tête fait deux fois plus de dégâts.
5. S'ils passent près de toi, ils te poursuivent et te frappent : les bords de l'écran rougissent. À 0 PV, « Tu es tombé ! » s'affiche et tu reviens devant le Cœur après 5 s (3 s de plus à chaque nouvelle chute).
6. **Vérifie surtout que les zombies marchent dans le bon sens et sont bien animés** (marche, course, coups, chute en mourant), et envoie une capture si l'un d'eux glisse, flotte, marche en crabe ou de dos. Sans l'étape 2, ce sont de simples cylindres gris.

## 11. Tester l'étape 6 : les vagues

1. En haut au milieu : le niveau, la vague (sur 10) et la phase. La partie commence par 40 s de **préparation** (25 s ensuite) ; **Entrée** lance la vague sans attendre.
2. Pendant l'**assaut**, le compteur montre les zombies qui restent. La composition des vagues est celle de Godot : plus de coureurs et de brutes au fil des vagues, des cracheurs dès la vague 3, des hurleurs dès la 4, des fouisseurs dès la 5, et un boss aux nuits de siège (vagues 5 et 10, en rouge). Leurs talents arrivent à l'étape 8.
3. Chaque zombie abattu rapporte des points et laisse un petit tas de **ferraille** doré : passe dessus (elle vient à toi à moins de 2,5 m). La ferraille et les points sont en haut à gauche. Elle disparaît au bout de 30 s.
4. Vague repoussée : ta santé revient au maximum, tu reçois une prime de ferraille, puis 12 s de **récolte** pour ramasser le reste (Entrée pour passer).
5. Les messages de la partie (vague qui arrive, nouveau type de zombie...) s'affichent quelques secondes sous le panneau du haut.
6. **Défaite** si le Cœur tombe, **victoire** après la vague 10. Entrée recommence le niveau, ou passe au suivant après une victoire (ou clique sur les boutons ; le menu principal arrive à l'étape 9).
7. Le choix d'implant après les nuits de siège n'est pas encore là. La ferraille sert aux tours et aux réparations (étape 7, juste en dessous).

## 12. Tester l'étape 7 : les tours

1. Approche-toi d'un **ancrage** (dalle carrée au bord du chemin) et vise-le : des cartes apparaissent en bas, une par tour, avec leur prix en ferraille et en énergie. **C** ou la **molette** choisit la tour, **E** la construit. Un anneau au sol montre sa portée.
2. Les six tours de Godot, mêmes prix et mêmes chiffres : Mitrailleuse (25), Cryo (30, ralentit), Lance-flammes (35, met le feu), Arc électrique (40, saute de zombie en zombie), Mortier (45, tire en cloche et étourdit) et Phare (30, marque les zombies et renforce les tours dans son halo).
3. **Énergie** : 6 au départ (en haut à gauche). Chaque tour allumée en consomme 1 à 3 ; sans assez d'énergie, la tour est posée mais éteinte. Vise une tour et appuie sur **X** pour l'allumer ou l'éteindre.
4. Vise une tour et appuie sur **E** pour l'améliorer (3 niveaux) : elle grossit un peu, tire plus fort et plus loin.
5. **F** marque le zombie visé (« MARQUÉ » au-dessus de lui, même derrière un mur) : les tours le visent en premier et il prend 25 % de dégâts en plus.
6. **Combos**, comme dans Godot : pistolet sur un zombie gelé = **BRISÉ** (dégâts x3) ; tir sur un zombie chargé par l'Arc = **SURCHARGE** (onde qui blesse et étourdit autour) ; obus de Mortier sur un zombie en feu = **EMBRASEMENT**. Un petit texte s'affiche à chaque fois. Sous la barre de vie, des points de couleur montrent les états : bleu gelé, orange en feu, violet chargé, jaune étourdi.
7. **Réparer** : va près d'une barrière abîmée (ou du Cœur) et **maintiens E** : 1 ferraille pour 10 PV. Une barrière détruite se relève quand elle a retrouvé 30 % de ses PV.
8. Les tours n'ont pas encore de vraies flammes ni de glace (seulement des lumières et des traits lumineux) : les effets viendront plus tard. Dis-moi si une tour vise de travers ou si son canon pointe dans le mauvais sens.

## 13. Tester l'étape 8 : les zombies spéciaux

Ils arrivent à partir de la vague 3 : **Entrée** lance chaque vague sans attendre la fin du compte à rebours.

1. **Cracheur** (dès la vague 3) : il s'arrête à distance de la barrière qui le bloque (ou de toi si tu es à moins de 11 m), se cambre et crache une boule d'acide verte en cloche. Elle s'écrase sur le premier obstacle et laisse une flaque qui rétrécit. Elle fait mal à la barrière ou à toi, jamais aux tours.
2. **Hurleur** (dès la vague 4) : toutes les 5 s, il s'arrête et crie. Une onde orange s'élargit au sol, et les zombies à moins de 10 m deviennent plus rapides et plus forts pendant 6 s (point rouge sous leur barre de vie).
3. **Fouisseur** (dès la vague 5) : arrivé devant une barrière debout, il s'enfonce dans le sol, avance sous terre (une butte de terre se déplace) et ressort juste derrière. Sous terre, ni les balles ni les tours ne le touchent, mais un **Phare** allumé le fait sortir (« DÉBUSQUÉ ! »).
4. Le **boss** des nuits de siège ne se laisse pas étourdir (Mortier, SURCHARGE).
5. Dis-moi si un zombie spécial reste bloqué, s'enterre dans un mur ou ressort sous le sol.

## 14. Tester l'étape 9 : menu, choix du niveau et pause

1. **▶ Play** ouvre le **menu principal**, devant la carte : JOUER, QUITTER, ton record et les niveaux débloqués. La souris est libre.
2. **JOUER** ouvre le choix du niveau : six chapitres, cinq niveaux chacun. Au début, seul le **1** du chapitre 1 est ouvert (en orange). Gagner un niveau le met en vert et ouvre le suivant ; gagner le 5e ouvre le chapitre suivant. **Entrée**, dans le menu, lance directement le dernier niveau ouvert.
3. En partie, **P** (ou **Échap** dans le jeu exporté ; dans l'éditeur, Échap arrête la partie) met en **pause** : REPRENDRE, RECOMMENCER LE NIVEAU, MENU PRINCIPAL, QUITTER LE JEU. Tout est figé pendant la pause.
4. En fin de partie, deux boutons : NIVEAU SUIVANT (ou RECOMMENCER) et MENU PRINCIPAL, avec « Nouveau record ! » ou « Niveau suivant débloqué ! » quand c'est le cas.
5. La progression et le record sont sauvegardés sur ton PC (dossier `Saved/SaveGames` du projet) : ferme Unreal, rouvre-le, ils doivent être encore là. Pour repartir de zéro, supprime le fichier `Progression.sav`.
6. Les chapitres 2 à 6 utilisent leur propre carte ; leur décor arrive à l'étape 15.

## 15. Tester l'étape 10 : réglages et touches

1. Dans le menu principal ou la pause, le nouveau bouton **RÉGLAGES** ouvre une page avec, en haut, **−** et **+** pour la sensibilité de la souris, le volume et le champ de vision, puis « Inverser l'axe vertical » (OUI / NON) et la qualité graphique (BASSE, MOYENNE, HAUTE, ÉPIQUE).
2. La sensibilité **1,00 x** correspond à la souris que tu avais validée (déjà baissée de 10 %). Change-la, reprends la partie : la vue doit tourner plus ou moins vite. Pareil pour le volume, le champ de vision et l'axe inversé.
3. La qualité graphique change tout de suite. En BASSE et MOYENNE, Unreal coupe Lumen (l'éclairage réaliste) : c'est plus rapide mais plus sombre et moins beau. Dans l'éditeur, ce choix s'applique aussi à la fenêtre d'Unreal.
4. En dessous, les **touches** : clique sur une touche (elle affiche « ... »), puis appuie sur la nouvelle touche ou un bouton de souris. Si elle était déjà prise, les deux actions échangent leurs touches. Échap annule l'attente (dans l'éditeur, Échap arrête la partie : clique plutôt sur une autre touche). **TOUCHES PAR DÉFAUT** remet l'AZERTY d'origine.
5. Les textes d'aide du jeu (rappel des touches, « E construire », « [Entrée] lancer la vague »...) affichent tes nouvelles touches. Échap reste toujours la pause et la molette change toujours d'arme ou de tour.
6. Ferme Unreal et rouvre-le : les réglages et les touches doivent être restés (même fichier `Progression.sav`).

## 16. Tester l'étape 11 : insignes et améliorations permanentes

1. Chaque partie rapporte des **insignes** : 1 par vague repoussée (un peu plus dans les niveaux durs), plus 5 en cas de victoire. L'écran de fin affiche « +N insignes » et le total à dépenser.
2. Dans le menu principal, le nouveau bouton **AMÉLIORATIONS** ouvre les sept améliorations de la version Godot : Vitalité, Armurier, Ingénieur, Maçonnerie, Réserves, Récupération, Dynamo. Chaque case orange est un rang acheté.
3. **ACHETER** dépense les insignes (le bouton est grisé s'il en manque). **TOUT REMBOURSER** rend tous les insignes dépensés.
4. Les achats comptent dès la partie suivante : par exemple Réserves donne 65 ferraille au départ au lieu de 55, Vitalité monte la barre de vie au-dessus de 100, Dynamo ajoute 1 à l'énergie.
5. Insignes et améliorations sont gardés dans le même fichier `Progression.sav`.

## 17. Tester l'étape 12 : les implants

1. Repousse la **Nuit de siège** de la vague 5 : la partie se fige et la fenêtre « SIÈGE REPOUSSÉ » propose trois implants, chacun avec un bonus (en vert) et un malus (en rouge). Clique sur l'un d'eux.
2. La partie reprend avec la récolte, et « Implant installé : ... » s'affiche. Tes implants sont rappelés au-dessus de ta barre de vie.
3. Les quatre implants de la version Godot :
   - **Cœur de batterie** : +2 énergie pour les tours, mais tu cours 15 % moins vite.
   - **Sang froid** : BRISÉ (pistolet sur un zombie gelé) fait x5 au lieu de x3, mais le fusil fait 20 % de dégâts en moins.
   - **Charognard** : la ferraille vient à toi de 12 m, mais tu n'as que 80 PV au lieu de 100.
   - **Main lourde** : pistolet +50 % de dégâts, mais rechargements 50 % plus lents.
4. Les implants ne valent que pour la partie en cours.

## 18. Tester l'étape 13 : l'établi

1. L'**établi** est à côté du Cœur : au départ, retourne-toi, il est sur ta droite, avec « ÉTABLI » écrit en vert au-dessus. Approche-toi, vise-le : l'aide propose « E ouvrir l'établi ». Pendant l'assaut, il est fermé (« utilisable seulement entre les vagues »).
2. **E** l'ouvre et fige la partie. **E**, **Échap**, **P** ou **FERMER** le referment.
3. Pour chaque arme : **Canon** (+20 % de dégâts par niveau, 40 puis 80 ferraille), **Chargeur** (+30 % de balles, 30 puis 60 ; le nouveau chargeur arrive plein) et quatre **munitions** (60 ferraille chacune la première fois, gratuites ensuite) :
   - **Incendiaires** : mettent le feu (-10 % de dégâts), traçante orange.
   - **Perforantes** : traversent jusqu'à 3 zombies, +25 % sur Brutes et Boss.
   - **Électriques** : chargent la cible ; un tir normal ensuite déclenche la SURCHARGE.
4. **Générateur** : +2 énergie par niveau (50, 90, 140 ferraille), donc plus de tours allumées en même temps.
5. Tout repart de zéro à chaque partie.

## 19. Tester l'étape 14 : les gadgets

1. En bas à gauche, au-dessus de ta vie : « [A] Barricade   [G] Leurre sonore ». En orange, le gadget est prêt ; en gris, il affiche sa recharge en secondes.
2. **A : Barricade**. Va près du chemin des zombies et regarde-le : elle se déplie en travers, 3 m devant toi (250 PV, +10 % par vague). Les zombies s'arrêtent pour la casser. Au-dessus : « Barricade 250 PV 20 s ». Elle disparaît au bout de 20 s, ou s'effondre quand elle cède. Recharge : 35 s. Loin du chemin ou sur une autre barrière, un message explique pourquoi elle ne se pose pas.
3. **G : Leurre sonore**. Il part en cloche là où tu vises (14 m au plus), bipe et clignote en rouge pendant 8 s. Les zombies à moins de 16 m foncent dessus et le frappent (120 PV). Recharge : 30 s.
4. **Drone de récolte** : à choisir à l'établi (carte **GADGETS**, un bouton par gadget pour chaque touche ; mettre le même gadget sur les deux touches les échange). Il décolle au-dessus de toi, va chercher la ferraille à moins de 25 m pendant 25 s (un « + » vert s'affiche à chaque pièce ramassée), puis revient et disparaît. Recharge : 45 s.
5. Les touches **A** et **G** se changent dans les réglages, comme les autres.

## 20. Tester l'étape 15 : le décor des autres chapitres

Pour voir un chapitre sans avoir gagné les précédents : pendant la partie, appuie sur **²** (ou **F10**) pour ouvrir la console en bas de l'écran, tape `open Entry?Niveau=21` puis **Entrée**. Le nombre est le niveau : 6 = chapitre 2, 11 = chapitre 3, 16 = chapitre 4, 21 = chapitre 5, 26 = chapitre 6. Ta progression n'est pas changée.

1. **Le Marais** (niveaux 6 à 10) : mares d'eau noire qui reflètent le ciel, touffes de roseaux secs, arbres morts à l'écorce verdâtre.
2. **La Raffinerie** (11 à 15) : conteneurs maritimes rouillés (parfois deux empilés), grandes cuves avec une échelle et un feu rouge au sommet.
3. **Le Col Gelé** (16 à 20) : sapins couverts de neige, maisons en ruine aux murs effondrés en escalier, rochers plus gris.
4. **La Ville Morte** (21 à 25) : immeubles éventrés de 2 à 4 niveaux (fenêtres cassées ou condamnées, rideaux de fer, balcons, toit arraché), réverbères dont un sur trois grésille encore, flaques.
5. **La Route Brûlée** (26 à 30) : dunes de sable basses, poteaux télégraphiques penchés avec leurs câbles qui pendent, ruines.
6. On ne traverse ni les immeubles, ni les ruines, ni les cuves, ni les conteneurs, ni les sapins.

## 21. Tester l'étape 16 : l'herbe et les cailloux

1. Relance d'abord le script d'import, comme à l'étape 2 (`py importer_ressources.py` dans la barre **Cmd**) : il crée en plus le matériau de l'herbe, **M_Herbe**. Sans lui, l'herbe s'affiche quand même, mais immobile et d'une seule couleur.
2. De l'herbe sèche pousse en plaques sur toute la carte, plus foncée au pied qu'à la pointe, et elle ondule au vent. Il n'y en a ni sur le chemin, ni autour des ancrages et des barrières, ni dans la base autour du Cœur.
3. De petits cailloux sont semés partout, plus serrés au bord du chemin, ce qui adoucit ses bords. Dans la Raffinerie et la Ville Morte, ce sont des éclats de béton anguleux.
4. Chaque chapitre a son herbe : haute et vert sombre dans le Marais, courte et pâle sous la neige du Col Gelé, broussailles claires sur la Route Brûlée (console : `open Entry?Niveau=6`, `16` ou `26`, voir l'étape 15).
5. On marche à travers sans être gêné. Au-delà de 85 m environ, l'herbe et les cailloux ne sont plus dessinés. Si le jeu rame depuis cette étape, dis-le dans le fil.

## 22. Tester l'étape 17 : la météo et le vent

1. Relance le script d'import (`py importer_ressources.py` dans la barre **Cmd**) : il crée le matériau **M_Meteo** et règle le vent et la pluie pour qu'ils tournent en boucle sans coupure. Sans lui, les flocons et les gouttes sont opaques.
2. **La Brèche** (chapitre 1) : pas de météo, seulement le vent qu'on entend en fond.
3. **Le Marais** (`open Entry?Niveau=6`) : quelques lucioles vertes flottent autour de toi, s'allument puis s'éteignent.
4. **La Raffinerie** (`11`) : des cendres grises tombent lentement en dérivant.
5. **Le Col Gelé** (`16`) : la neige tombe en biais, poussée par le vent.
6. **La Ville Morte** (`21`) : pluie fine en traits presque verticaux, et on entend la pluie par-dessus le vent.
7. **La Route Brûlée** (`26`) : de la poussière file presque à l'horizontale au ras du sol, et le vent est plus fort.
8. La météo te suit partout sur la carte. Le volume des réglages change aussi celui du vent et de la pluie.

## 23. Si ça ne marche pas

- **« DernierRempart could not be compiled. Try rebuilding from source manually. »** :
  1. Clic droit sur **DernierRempart.uproject** (sous Windows 11 : « Afficher d'autres options »), puis **Generate Visual Studio project files**.
  2. Ouvre le fichier **DernierRempart.sln** qui vient d'apparaître (avec Visual Studio).
  3. Menu **Générer**, puis **Générer la solution**.
  4. Ouvre la **Liste d'erreurs** (menu Affichage) et copie les premières lignes rouges dans le fil.
- **Un autre message** : fais une capture d'écran de la fenêtre et envoie-la dans le fil.

## Bon à savoir

- Les dossiers `Binaries`, `Intermediate`, `Saved` et `DerivedDataCache` sont créés par Unreal sur ton PC. Ils ne partent pas sur GitHub (ils sont ignorés), donc GitHub Desktop ne te les proposera pas.
- La version Godot n'est pas touchée : tu peux continuer à y jouer comme avant.
