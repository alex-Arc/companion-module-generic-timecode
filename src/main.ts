import { InstanceBase, InstanceStatus, runEntrypoint, SomeCompanionConfigField } from '@companion-module/base'
import { GetConfigFields, type ModuleConfig } from './config.js'
import { UpdateVariableDefinitions, VariableIds } from './variables.js'
import { UpdateActions } from './actions.js'
import { UpdateFeedbacks } from './feedbacks.js'

import { AudioIO, SampleFormat8Bit, IoStreamRead, getDevices } from 'naudiodon'

import bindings from 'bindings'
import { UpgradeScripts } from './upgrades.js'

const addon = bindings('LtcDecoderRead')

function padTo2Digits(number: number) {
	return number.toString().padStart(2, '0')
}

type LTCReturn = {
	f: number
	s: number
	m: number
	h: number
	fps: number
	discontinuity: boolean
}

export class ModuleInstance extends InstanceBase<ModuleConfig> {
	config!: ModuleConfig // Setup in init()
	audioInterface!: IoStreamRead
	pendingDiscontinuity = 0
	discontinuityCount = 0
	fps = 0
	status: 'none' | 'locked' | 'lost' = 'none'
	statusCounter = 0
	offset = 0

	constructor(internal: unknown) {
		super(internal)
	}

	async init(config: ModuleConfig): Promise<void> {
		this.config = config

		// this.audioInterface.resume()

		this.updateStatus(InstanceStatus.Connecting)

		this.updateActions() // export actions
		this.updateFeedbacks() // export feedbacks
		this.updateVariableDefinitions() // export variable definitions

		this.setVariableValues({
			[VariableIds.Status]: this.status,
			[VariableIds.Discontinuity]: false,
			[VariableIds.DiscontinuityCount]: 0,
			[VariableIds.TimeCode_HMSF]: '--:--:--.--',
			[VariableIds.Fps]: 0,
		})

		this.startCapture()
	}
	// When module gets deleted
	async destroy(): Promise<void> {
		this.stopCapture()
		this.log('debug', 'destroy')
	}

	async configUpdated(config: ModuleConfig): Promise<void> {
		this.config = config
		this.updateStatus(InstanceStatus.Connecting)

		this.stopCapture()
		this.startCapture()
	}

	// Return config fields for web config
	getConfigFields(): SomeCompanionConfigField[] {
		const interfaceOptions = getDevices().filter((opt) => opt.maxInputChannels > 1)
		return GetConfigFields(interfaceOptions)
	}

	updateActions(): void {
		UpdateActions(this)
	}

	updateFeedbacks(): void {
		UpdateFeedbacks(this)
	}

	updateVariableDefinitions(): void {
		UpdateVariableDefinitions(this)
	}

	stopCapture(): void {
		this.audioInterface.quit()
		this.audioInterface.removeAllListeners()
	}

	startCapture(): void {
		try {
			this.audioInterface = AudioIO({
				inOptions: {
					channelCount: 1,
					sampleFormat: SampleFormat8Bit,
					framesPerBuffer: 0,
					highwaterMark: 1024,
					deviceId: 23, // Use -1 or omit the deviceId to select the default device
					closeOnError: true, // Close the stream if an audio error is detected, if set false then just log the error
				},
			})
		} catch (e) {
			this.log('error', JSON.stringify(e))
			this.updateStatus(InstanceStatus.BadConfig)
			return
		}

		this.audioInterface.on('data', (buf: Buffer) => {
			const ret: LTCReturn = addon.LtcDecoderRead(buf, this.offset)
			this.offset += buf.length

			const currentStatus = this.status
			if (ret) {
				this.statusCounter = 0
				//TODO: ignore initial discontinuity warning until locked
				const formattedTime = `${padTo2Digits(ret.h)}:${padTo2Digits(ret.m)}:${padTo2Digits(ret.s)}:${padTo2Digits(ret.f)}`
				this.setVariableValues({ [VariableIds.TimeCode_HMSF]: formattedTime })
				this.status = 'locked'

				if (ret.fps != this.fps) {
					this.fps = ret.fps
					this.setVariableValues({ [VariableIds.Fps]: this.fps })
				}
				if (ret.discontinuity) {
					this.pendingDiscontinuity = 100
					this.discontinuityCount++
					this.setVariableValues({
						[VariableIds.Discontinuity]: true,
						[VariableIds.DiscontinuityCount]: this.discontinuityCount,
					})
				}
				this.pendingDiscontinuity--
				if (this.pendingDiscontinuity <= 0) {
					this.setVariableValues({
						[VariableIds.Discontinuity]: false,
						[VariableIds.DiscontinuityCount]: this.discontinuityCount,
					})
				}
			} else {
				this.statusCounter++
			}
			if (this.statusCounter > 150) {
				this.status = 'none'
			} else if (this.statusCounter > 10) {
				this.status = 'lost'
			}

			if (currentStatus !== this.status) {
				this.setVariableValues({ [VariableIds.Status]: this.status })
			}
		})

		this.audioInterface.start()

		this.updateStatus(InstanceStatus.Ok)
	}
}

runEntrypoint(ModuleInstance, UpgradeScripts)
