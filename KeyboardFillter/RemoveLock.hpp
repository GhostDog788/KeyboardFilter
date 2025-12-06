#pragma once
#include <ntddk.h>
#include "Move.hpp"

namespace kstd {
	class RemoveLockGuard
	{
	friend class RemoveLock;
	public:
		RemoveLockGuard() = default;
		~RemoveLockGuard() {
			if (m_remove_lock) {
				IoReleaseRemoveLock(m_remove_lock, 0);
			}
		}
		NTSTATUS Lock(IO_REMOVE_LOCK* lock, PIRP Irp) {
			m_remove_lock = lock;
			auto status = IoAcquireRemoveLock(m_remove_lock, Irp);
			if (!NT_SUCCESS(status)) { // STATUS_DELETE_PENDING
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				m_remove_lock = nullptr;
			}
			return status;
		}
		void ReleaseAndWait() {
			if (m_remove_lock) {
				IoReleaseRemoveLockAndWait(m_remove_lock, 0);
				m_remove_lock = nullptr;
			}
		}
	private:
		IO_REMOVE_LOCK* m_remove_lock = nullptr;
	};

	class RemoveLock
	{
	public:
		RemoveLock() = default;
		void Initialize() {
			IoInitializeRemoveLock(&m_remove_lock, 'RmLk', 0, 0);
		}
		RemoveLockGuard LockAcquire(PIRP Irp, NTSTATUS& status) {
			RemoveLockGuard guard;
			status = guard.Lock(&m_remove_lock, Irp);
			return kstd::move(guard);
		}
	private:
		IO_REMOVE_LOCK m_remove_lock;
	};
}