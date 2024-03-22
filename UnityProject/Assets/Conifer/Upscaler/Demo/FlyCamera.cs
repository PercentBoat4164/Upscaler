using UnityEngine;

namespace Conifer.Upscaler.Demo
{
	[RequireComponent(typeof(Camera))]
	public class FlyCamera : MonoBehaviour {
		public float acceleration = 50; // how fast you accelerate
		public float accSprintMultiplier = 4; // how much faster you go when "sprinting"
		public float lookSensitivity = 1; // mouse look sensitivity
		public float dampingCoefficient = 5; // how quickly you break to a halt after you stop your input
		private Camera _camera;

		private Vector3 _velocity; // current velocity

		private static bool Focused {
			get => Cursor.lockState == CursorLockMode.Locked;
			set {
				Cursor.lockState = value ? CursorLockMode.Locked : CursorLockMode.None;
				Cursor.visible = value == false;
			}
		}

		private void OnEnable() => _camera = GetComponent<Camera>();

		private void OnDisable() => Focused = false;

		private void Update() {
			// Input
			if (Display.activeEditorGameViewTarget != _camera.targetDisplay) return;
			if (Focused)
				UpdateInput();
			else if (Input.GetMouseButtonDown(0))
				Focused = true;

			// Physics
			_velocity = Vector3.Lerp(_velocity, Vector3.zero, dampingCoefficient * Time.deltaTime);
			transform.position += _velocity * Time.deltaTime;
		}

		private void UpdateInput() {
			// Position
			_velocity += GetAccelerationVector() * Time.deltaTime;

			// Rotation
			var mouseDelta = lookSensitivity * new Vector2(Input.GetAxis("Mouse X"), -Input.GetAxis("Mouse Y"));
			var rotation = transform.rotation;
			var horiz = Quaternion.AngleAxis(mouseDelta.x, Vector3.up);
			var vert = Quaternion.AngleAxis(mouseDelta.y, Vector3.right);
			transform.rotation = horiz * rotation * vert;

			// Leave cursor lock
			if (Input.GetKeyDown(KeyCode.Escape))
				Focused = false;
		}

		private Vector3 GetAccelerationVector() {
			Vector3 moveInput = default;

			AddMovement(KeyCode.W, Vector3.forward);
			AddMovement(KeyCode.S, Vector3.back);
			AddMovement(KeyCode.D, Vector3.right);
			AddMovement(KeyCode.A, Vector3.left);
			AddMovement(KeyCode.Space, Vector3.up);
			AddMovement(KeyCode.LeftControl, Vector3.down);
			var direction = transform.TransformVector(moveInput.normalized);

			if (Input.GetKey(KeyCode.LeftShift))
				return direction * (acceleration * accSprintMultiplier); // "sprinting"
			return direction * acceleration; // "walking"

			void AddMovement(KeyCode key, Vector3 dir) {
				if (Input.GetKey(key))
					moveInput += dir;
			}
		}
	}
}