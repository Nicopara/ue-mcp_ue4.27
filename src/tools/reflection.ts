import { z } from "zod";
import { categoryTool, bp, type ToolDef } from "../types.js";

export const reflectionTool: ToolDef = categoryTool(
  "reflection",
  "UE reflection: classes, structs, enums, gameplay tags.",
  {
    reflect_class:  bp("Reflect UClass. Params: className, includeInherited?", "reflect_class"),
    reflect_struct: bp("Reflect UScriptStruct. Params: structName", "reflect_struct"),
    reflect_enum:   bp("Reflect UEnum. Params: enumName", "reflect_enum"),
    list_classes:   bp("List classes. Params: parentFilter?, limit?", "list_classes"),
    list_tags:      bp("List gameplay tags. Params: filter?", "list_gameplay_tags"),
    create_tag:     bp("Create gameplay tag. Params: tag, comment?", "create_gameplay_tag"),
  },
  undefined,
  {
    className: z.string().optional(),
    includeInherited: z.boolean().optional(),
    structName: z.string().optional(),
    enumName: z.string().optional(),
    parentFilter: z.string().optional(),
    limit: z.number().optional(),
    filter: z.string().optional(),
    tag: z.string().optional(),
    comment: z.string().optional(),
  },
);
