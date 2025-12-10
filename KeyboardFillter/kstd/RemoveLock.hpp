#pragma once
#include <ntddk.h>
#include "Move.hpp"

namespace kstd {

	class RemoveLockGuard
	{
	friend class RemoveLock;
	public:
		RemoveLockGuard() = default;
		RemoveLockGuard(const RemoveLockGuard& other) = delete;
		RemoveLockGuard& operator=(RemoveLockGuard& other) = delete;
		RemoveLockGuard(RemoveLockGuard&& other) {
			m_remove_lock = other.m_remove_lock;
			other.m_remove_lock = nullptr;
		}
		RemoveLockGuard& operator=(RemoveLockGuard&& other) {
			if (this != &other) {
				m_remove_lock = other.m_remove_lock;
				other.m_remove_lock = nullptr;
			}
			return *this;
		}
		~RemoveLockGuard() {
			Unlock();
		}
		NTSTATUS Lock(PIO_REMOVE_LOCK lock, PIRP Irp) {
			m_remove_lock = lock;
			auto status = IoAcquireRemoveLock(m_remove_lock, NULL);
			if (!NT_SUCCESS(status)) { // STATUS_DELETE_PENDING
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				m_remove_lock = nullptr;
			}
			return status;
		}
		void Unlock() {
			if (m_remove_lock) {
				IoReleaseRemoveLock(m_remove_lock, NULL);
				m_remove_lock = nullptr;
			}
		}
		void Clear() {
			m_remove_lock = nullptr;
		}
	private:
		PIO_REMOVE_LOCK m_remove_lock = nullptr;
	};

	class RemoveLock
	{
	public:
		RemoveLock() = default;
		void Initialize() {
			IoInitializeRemoveLock(&m_remove_lock, 'RmLk', 0, 0);
		}
		RemoveLockGuard&& LockAcquire(PIRP Irp, NTSTATUS& status) {
			RemoveLockGuard guard;
			status = guard.Lock(&m_remove_lock, Irp);
			return kstd::move(guard);
		}
		NTSTATUS ManualLock() {
			return IoAcquireRemoveLock(&m_remove_lock, NULL);
		}
		NTSTATUS ManualLock(PIRP Irp) {
			auto status = IoAcquireRemoveLock(&m_remove_lock, NULL);
			if (!NT_SUCCESS(status)) { // STATUS_DELETE_PENDING
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
			}
			return status;
		}
		void ManualUnlock() {
			IoReleaseRemoveLock(&m_remove_lock, NULL);
		}
		void ReleaseAndWait() {
			IoReleaseRemoveLockAndWait(&m_remove_lock, NULL);
		}
	private:
		IO_REMOVE_LOCK m_remove_lock;
	};
}
