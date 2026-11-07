# -*- coding: utf-8 -*-
"""Importe dans Unreal les textures et les modèles de la version Godot (dossier assets/ du dépôt).

À lancer dans l'éditeur Unreal, une fois au début puis chaque fois que les textures ou les modèles
changent dans la version Godot :
  - en bas de l'éditeur, dans la barre « Cmd » (choisir « Python » à gauche si besoin), taper :
        py importer_ressources.py
  - ou menu Outils (Tools) > Exécuter un script Python (Execute Python Script), puis choisir ce fichier.

Tout est rangé dans /Game/Importe (dossier Content/Importe sur le disque). Ce dossier n'est pas
envoyé sur GitHub : on peut le supprimer et relancer le script pour tout refaire.

Ce que fait le script :
  1. importe les textures de assets/textures (couleur, relief, rugosité) dans /Game/Importe/Textures ;
  2. crée le matériau M_Surface, qui plaque ces textures selon la position dans le monde (comme le
     « triplanar » de Godot), et un matériau MI_<nom> par jeu de textures (béton, brique, métal...) ;
  3. importe les armes et les zombies (assets/models/*.gltf, avec leurs squelettes et leurs
     animations) dans /Game/Importe/Modeles/<nom>.
"""

import os

import unreal

DOSSIER = "/Game/Importe"
DOSSIER_TEXTURES = DOSSIER + "/Textures"
DOSSIER_MATERIAUX = DOSSIER + "/Materiaux"
DOSSIER_MODELES = DOSSIER + "/Modeles"

# Jeux de textures de assets/textures : <nom>_albedo.png, <nom>_normal.png, et <nom>_roughness.png
# quand il existe (sinon la rugosité vaut 1, comme dans Godot).
JEUX = ["beton", "brick", "cloth", "concrete", "ground", "metal", "mud", "plaster", "skin"]

# Plaquage dans le monde : on prend les deux axes du plan le plus face à la surface (sol, mur avant
# ou mur de côté), en mètres, fois l'échelle (Godot : uv1_scale, nombre de répétitions par mètre).
CODE_PLAQUAGE = """float3 a = abs(N);
float2 uv;
if (a.z >= a.x && a.z >= a.y) { uv = P.xy; }
else if (a.x >= a.y) { uv = float2(P.y, -P.z); }
else { uv = float2(P.x, -P.z); }
return uv * (Echelle / 100.0);"""

outils = unreal.AssetToolsHelpers.get_asset_tools()
bibli = unreal.EditorAssetLibrary
mat_lib = unreal.MaterialEditingLibrary


def dossier_assets():
    projet = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    racine = os.path.normpath(os.path.join(projet, "..", "..", "assets"))
    if not os.path.isdir(racine):
        raise RuntimeError("Dossier introuvable : " + racine + " (le projet doit rester dans dernier-rampart/unreal/DernierRempart).")
    return racine


def importer(fichier, destination, nom=None):
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", destination)
    if nom:
        tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("save", True)
    outils.import_asset_tasks([tache])
    chemins = list(tache.get_editor_property("imported_object_paths"))
    if not chemins:
        unreal.log_error("DernierRempart : import raté pour " + fichier)
    return chemins


def importer_texture(fichier, nom, genre):
    importer(fichier, DOSSIER_TEXTURES, nom)
    texture = unreal.load_asset(DOSSIER_TEXTURES + "/" + nom)
    if texture is None:
        return None
    if genre == "normal":
        # Godot range le relief à la manière d'OpenGL, Unreal à la manière de DirectX : on inverse le vert.
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("flip_green_channel", True)
    elif genre == "rugosite":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("srgb", True)
    bibli.save_loaded_asset(texture)
    return texture


def importer_textures(racine):
    textures = {}
    for jeu in JEUX:
        textures[jeu] = {}
        for suffixe, genre in (("albedo", "couleur"), ("normal", "normal"), ("roughness", "rugosite")):
            fichier = os.path.join(racine, "textures", jeu + "_" + suffixe + ".png")
            if os.path.isfile(fichier):
                textures[jeu][genre] = importer_texture(fichier, "T_" + jeu + "_" + suffixe, genre)
        unreal.log("DernierRempart : textures « " + jeu + " » importées")
    return textures


def charger_ou_creer(nom, dossier, classe, fabrique):
    chemin = dossier + "/" + nom
    if bibli.does_asset_exist(chemin):
        return unreal.load_asset(chemin)
    return outils.create_asset(nom, dossier, classe, fabrique)


def noeud(materiau, classe, x, y):
    return mat_lib.create_material_expression(materiau, classe, x, y)


def creer_materiau(defauts):
    materiau = charger_ou_creer("M_Surface", DOSSIER_MATERIAUX, unreal.Material, unreal.MaterialFactoryNew())
    mat_lib.delete_all_material_expressions(materiau)

    position = noeud(materiau, unreal.MaterialExpressionWorldPosition, -1400, 0)
    normale_monde = noeud(materiau, unreal.MaterialExpressionVertexNormalWS, -1400, 150)
    echelle = noeud(materiau, unreal.MaterialExpressionScalarParameter, -1400, 300)
    echelle.set_editor_property("parameter_name", "Echelle")
    echelle.set_editor_property("default_value", 0.25)

    plaquage = noeud(materiau, unreal.MaterialExpressionCustom, -1100, 100)
    plaquage.set_editor_property("description", "Plaquage monde")
    plaquage.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    plaquage.set_editor_property("code", CODE_PLAQUAGE)
    entrees = []
    for nom in ("P", "N", "Echelle"):
        entree = unreal.CustomInput()
        entree.set_editor_property("input_name", nom)
        entrees.append(entree)
    plaquage.set_editor_property("inputs", entrees)
    mat_lib.connect_material_expressions(position, "", plaquage, "P")
    mat_lib.connect_material_expressions(normale_monde, "", plaquage, "N")
    mat_lib.connect_material_expressions(echelle, "", plaquage, "Echelle")

    def echantillon(nom, type_, texture, y):
        e = noeud(materiau, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        e.set_editor_property("parameter_name", nom)
        e.set_editor_property("texture", texture)
        e.set_editor_property("sampler_type", type_)
        mat_lib.connect_material_expressions(plaquage, "", e, "UVs")
        return e

    couleur = echantillon("Couleur", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, defauts["couleur"], -300)
    relief = echantillon("Normale", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, defauts["normal"], 50)
    rugosite = echantillon("Rugosite", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, defauts["rugosite"], 400)

    teinte = noeud(materiau, unreal.MaterialExpressionVectorParameter, -700, -550)
    teinte.set_editor_property("parameter_name", "Teinte")
    teinte.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    multiplier = noeud(materiau, unreal.MaterialExpressionMultiply, -350, -400)
    mat_lib.connect_material_expressions(couleur, "RGB", multiplier, "A")
    mat_lib.connect_material_expressions(teinte, "", multiplier, "B")
    mat_lib.connect_material_property(multiplier, "", unreal.MaterialProperty.MP_BASE_COLOR)

    mat_lib.connect_material_property(relief, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # Jeux sans texture de rugosité (tissu, peau) : RugositeFixe remplace la texture quand
    # MelangeRugositeFixe vaut 1.
    fixe = noeud(materiau, unreal.MaterialExpressionScalarParameter, -700, 700)
    fixe.set_editor_property("parameter_name", "RugositeFixe")
    fixe.set_editor_property("default_value", 1.0)
    melange = noeud(materiau, unreal.MaterialExpressionScalarParameter, -700, 800)
    melange.set_editor_property("parameter_name", "MelangeRugositeFixe")
    melange.set_editor_property("default_value", 0.0)
    choix = noeud(materiau, unreal.MaterialExpressionLinearInterpolate, -350, 500)
    mat_lib.connect_material_expressions(rugosite, "R", choix, "A")
    mat_lib.connect_material_expressions(fixe, "", choix, "B")
    mat_lib.connect_material_expressions(melange, "", choix, "Alpha")
    mat_lib.connect_material_property(choix, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = noeud(materiau, unreal.MaterialExpressionScalarParameter, -350, 250)
    metal.set_editor_property("parameter_name", "Metal")
    metal.set_editor_property("default_value", 0.0)
    mat_lib.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    mat_lib.layout_material_expressions(materiau)
    mat_lib.recompile_material(materiau)
    bibli.save_loaded_asset(materiau)
    unreal.log("DernierRempart : matériau M_Surface créé")
    return materiau


def creer_instances(materiau, textures, defauts):
    for jeu, tex in textures.items():
        if "couleur" not in tex or "normal" not in tex:
            continue
        instance = charger_ou_creer("MI_" + jeu, DOSSIER_MATERIAUX, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mat_lib.set_material_instance_parent(instance, materiau)
        mat_lib.set_material_instance_texture_parameter_value(instance, "Couleur", tex["couleur"])
        mat_lib.set_material_instance_texture_parameter_value(instance, "Normale", tex["normal"])
        if tex.get("rugosite"):
            mat_lib.set_material_instance_texture_parameter_value(instance, "Rugosite", tex["rugosite"])
            mat_lib.set_material_instance_scalar_parameter_value(instance, "MelangeRugositeFixe", 0.0)
        else:
            mat_lib.set_material_instance_texture_parameter_value(instance, "Rugosite", defauts["rugosite"])
            mat_lib.set_material_instance_scalar_parameter_value(instance, "MelangeRugositeFixe", 1.0)
        mat_lib.update_material_instance(instance)
        bibli.save_loaded_asset(instance)
    unreal.log("DernierRempart : matériaux MI_* créés")


def importer_modeles(racine):
    dossier = os.path.join(racine, "models")
    for fichier in sorted(os.listdir(dossier)):
        if not fichier.endswith(".gltf"):
            continue
        nom = fichier[:-len(".gltf")]
        importer(os.path.join(dossier, fichier), DOSSIER_MODELES + "/" + nom)
        unreal.log("DernierRempart : modèle « " + nom + " » importé")


def main():
    racine = dossier_assets()
    unreal.log("DernierRempart : import depuis " + racine)
    textures = importer_textures(racine)
    defauts = {
        "couleur": textures["concrete"]["couleur"],
        "normal": textures["concrete"]["normal"],
        "rugosite": textures["concrete"]["rugosite"],
    }
    materiau = creer_materiau(defauts)
    creer_instances(materiau, textures, defauts)
    importer_modeles(racine)
    bibli.save_directory(DOSSIER, only_if_is_dirty=True, recursive=True)
    unreal.log("DernierRempart : import terminé. Relance la partie (Play) pour voir le résultat.")


main()
