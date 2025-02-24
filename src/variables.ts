import type { ModuleInstance } from './main.js'

export enum VariableIds {
	TimeCode_HMSF = 'timecode_hmsf',
	Fps = 'fps',
	Discontinuity = 'discontinuity',
	DiscontinuityCount = 'discontinuity_count',
	Status = 'status',
}

export function UpdateVariableDefinitions(self: ModuleInstance): void {
	self.setVariableDefinitions([
		{ variableId: VariableIds.TimeCode_HMSF, name: 'SMPTE TimeCode' },
		{ variableId: VariableIds.Fps, name: 'Current detected framerate' },
		{ variableId: VariableIds.Discontinuity, name: 'Detected discontinuity in timecode' },
		{ variableId: VariableIds.DiscontinuityCount, name: 'Number of detected discontinuity in timecode' },
		{ variableId: VariableIds.Status, name: 'Timecode status' },
	])
}
