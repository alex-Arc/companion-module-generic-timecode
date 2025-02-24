import { type SomeCompanionConfigField } from '@companion-module/base'
import { DeviceInfo } from 'naudiodon'

export interface ModuleConfig {
	host: string
	port: number
}

export function GetConfigFields(devices: DeviceInfo[]): SomeCompanionConfigField[] {
	return [
		{
			type: 'dropdown',
			id: 'interface',
			label: 'Audio Interface',
			choices: devices.map((device) => ({ id: device.id, label: device.name })).concat([{ id: -1, label: 'Default' }]),
			default: -1,
			width: 6,
		},
	]
}
