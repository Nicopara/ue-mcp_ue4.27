import type { TaskContext } from "flowkit";
import type { IBridge } from "../bridge.js";
import type { ProjectContext } from "../project.js";

export interface FlowContext extends TaskContext {
  bridge: IBridge;
  project: ProjectContext;
}
