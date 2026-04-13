import { z } from "zod";
import { categoryTool, bp, type ToolDef } from "../types.js";
import { Vec3, Rotator } from "../schemas.js";

export const niagaraTool: ToolDef = categoryTool(
  "niagara",
  "Niagara VFX: systems, emitters, spawning, parameters, and graph authoring.",
  {
    list:           bp("List Niagara assets. Params: directory?, recursive?", "list_niagara_systems"),
    get_info:       bp("Inspect system. Params: assetPath", "get_niagara_info"),
    spawn:          bp("Spawn VFX. Params: systemPath, location, rotation?, label?", "spawn_niagara_at_location"),
    set_parameter:  bp("Set parameter. Params: actorLabel, parameterName, value, parameterType?", "set_niagara_parameter"),
    create:         bp("Create system. Params: name, packagePath?", "create_niagara_system"),
    create_emitter: bp("Create Niagara emitter. Params: name, packagePath?, templatePath?", "create_niagara_emitter"),
    add_emitter:    bp("Add emitter to system. Params: systemPath, emitterPath", "add_emitter_to_system"),
    list_emitters:  bp("List emitters in system. Params: systemPath", "list_emitters_in_system"),
    set_emitter_property: bp("Set emitter property. Params: systemPath, emitterName?, propertyName, value", "set_emitter_property"),
    list_modules:   bp("List Niagara modules. Params: directory?", "list_niagara_modules"),
    get_emitter_info: bp("Inspect emitter. Params: assetPath", "get_emitter_info"),
  },
  undefined,
  {
    assetPath: z.string().optional(), actorLabel: z.string().optional(),
    directory: z.string().optional(), recursive: z.boolean().optional(),
    systemPath: z.string().optional(), emitterPath: z.string().optional(),
    location: Vec3.optional(),
    rotation: Rotator.optional(),
    label: z.string().optional(),
    parameterName: z.string().optional(),
    value: z.unknown().optional(),
    parameterType: z.string().optional(),
    name: z.string().optional(),
    packagePath: z.string().optional(),
    templatePath: z.string().optional(),
    emitterName: z.string().optional(),
    propertyName: z.string().optional(),
  },
);
