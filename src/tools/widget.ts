import { z } from "zod";
import { categoryTool, bp, type ToolDef } from "../types.js";

export const widgetTool: ToolDef = categoryTool(
  "widget",
  "UMG Widget Blueprints, Editor Utility Widgets, and Editor Utility Blueprints.",
  {
    read_tree:         bp("Read widget hierarchy. Params: assetPath", "read_widget_tree"),
    get_details:       bp("Inspect widget. Params: assetPath, widgetName", "get_widget_details"),
    set_property:      bp("Set widget property. Params: assetPath, widgetName, propertyName, value", "set_widget_property"),
    list:              bp("List Widget BPs. Params: directory?, recursive?", "list_widget_blueprints"),
    read_animations:   bp("Read UMG animations. Params: assetPath", "read_widget_animations"),
    create:            bp("Create Widget BP. Params: name, packagePath?, parentClass?", "create_widget_blueprint"),
    create_utility_widget:    bp("Create editor utility widget. Params: name, packagePath?", "create_editor_utility_widget"),
    run_utility_widget:       bp("Open editor utility widget. Params: assetPath", "run_editor_utility_widget"),
    create_utility_blueprint: bp("Create editor utility blueprint. Params: name, packagePath?", "create_editor_utility_blueprint"),
    run_utility_blueprint:    bp("Run editor utility blueprint. Params: assetPath", "run_editor_utility_blueprint"),
    add_widget:               bp("Add widget to widget tree. Params: assetPath, widgetClass, widgetName?, parentWidgetName?", "add_widget"),
    remove_widget:            bp("Remove widget from tree. Params: assetPath, widgetName", "remove_widget"),
    move_widget:              bp("Reparent widget. Params: assetPath, widgetName, newParentWidgetName", "move_widget"),
    list_classes:             bp("List available widget classes", "list_widget_classes"),
  },
  undefined,
  {
    assetPath: z.string().optional(),
    widgetName: z.string().optional(),
    widgetClass: z.string().optional(),
    parentWidgetName: z.string().optional(),
    newParentWidgetName: z.string().optional(),
    propertyName: z.string().optional(),
    value: z.unknown().optional(),
    directory: z.string().optional(),
    recursive: z.boolean().optional(),
    name: z.string().optional(),
    packagePath: z.string().optional(),
    parentClass: z.string().optional(),
  },
);
