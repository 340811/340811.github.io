using UnityEngine;

[RequireComponent(typeof(CharacterController))]
public class SimpleFPSController : MonoBehaviour
{
    [SerializeField] private float walkSpeed = 5f;
    [SerializeField] private float runSpeed = 8f;
    [SerializeField] private float jumpHeight = 1.5f;
    [SerializeField] private float gravity = -19.62f;

    [SerializeField] private float mouseSensitivity = 2f;
    [SerializeField] private float pitchMin = -90f;
    [SerializeField] private float pitchMax = 90f;

    [SerializeField] private float groundCheckRayStart = 0.1f;
    [SerializeField] private float groundCheckRayLen = 0.25f;
    [SerializeField] private LayerMask groundMask = ~0;

    [SerializeField] private float coyoteTime = 0.15f;
    [SerializeField] private float jumpBufferTime = 0.10f;

    private CharacterController controller;
    private Camera playerCamera;
    private Transform groundCheck;

    private float pitch;
    private float velocityY;
    private float lastGroundedTime = -99f;
    private float lastJumpPressTime = -99f;
    private bool jumpConsumedThisPress;

    private void Awake()
    {
        controller = GetComponent<CharacterController>();
        playerCamera = GetComponentInChildren<Camera>();
        SetupGroundCheck();
    }

    private void SetupGroundCheck()
    {
        Transform t = transform.Find("GroundCheck");
        if (t != null)
        {
            groundCheck = t;
            return;
        }

        GameObject go = new GameObject("GroundCheck");
        go.transform.SetParent(transform);
        float bottomY = controller.center.y - controller.height * 0.5f + 0.05f;
        go.transform.localPosition = new Vector3(0f, bottomY, 0f);
        go.transform.localRotation = Quaternion.identity;
        groundCheck = go.transform;
    }

    private void Start()
    {
        Cursor.lockState = CursorLockMode.Locked;
        Cursor.visible = false;
    }

    private void Update()
    {
        HandleMouseLook();
        HandleMovementAndJump();
    }

    private void HandleMouseLook()
    {
        float mx = Input.GetAxis("Mouse X") * mouseSensitivity;
        float my = Input.GetAxis("Mouse Y") * mouseSensitivity;
        transform.Rotate(Vector3.up, mx);
        pitch -= my;
        pitch = Mathf.Clamp(pitch, pitchMin, pitchMax);
        if (playerCamera != null)
            playerCamera.transform.localRotation = Quaternion.Euler(pitch, 0f, 0f);
    }

    private bool IsGrounded()
    {
        if (controller.isGrounded) return true;

        Vector3 origin = groundCheck.position + Vector3.up * groundCheckRayStart;
        float halfR = controller.radius * 0.5f;

        return Physics.Raycast(origin, Vector3.down, groundCheckRayLen, groundMask) ||
               Physics.Raycast(origin + transform.right * -halfR, Vector3.down, groundCheckRayLen, groundMask) ||
               Physics.Raycast(origin + transform.right * halfR, Vector3.down, groundCheckRayLen, groundMask);
    }

    private void HandleMovementAndJump()
    {
        bool grounded = IsGrounded();
        if (grounded)
        {
            lastGroundedTime = Time.time;
            if (velocityY < 0f)
                velocityY = -2f;
        }

        bool jumpPressed = Input.GetButtonDown("Jump") || Input.GetKeyDown(KeyCode.Space);
        if (jumpPressed && !jumpConsumedThisPress)
        {
            lastJumpPressTime = Time.time;
            jumpConsumedThisPress = true;
        }
        if (!jumpPressed)
            jumpConsumedThisPress = false;

        float speed = Input.GetKey(KeyCode.LeftShift) ? runSpeed : walkSpeed;
        float h = Input.GetAxis("Horizontal");
        float v = Input.GetAxis("Vertical");
        Vector3 move = transform.right * h + transform.forward * v;
        if (move.sqrMagnitude > 1f) move.Normalize();
        move *= speed;

        float timeSinceGrounded = Time.time - lastGroundedTime;
        float timeSinceJumpPress = Time.time - lastJumpPressTime;

        if (timeSinceGrounded <= coyoteTime && timeSinceJumpPress <= jumpBufferTime)
        {
            velocityY = Mathf.Sqrt(jumpHeight * -2f * gravity);
            lastGroundedTime = -99f;
            lastJumpPressTime = -99f;
        }

        velocityY += gravity * Time.deltaTime;

        move.y = velocityY;
        controller.Move(move * Time.deltaTime);
    }

    private void OnDrawGizmosSelected()
    {
        if (groundCheck == null || controller == null) return;

        Vector3 origin = groundCheck.position + Vector3.up * groundCheckRayStart;
        float halfR = controller.radius * 0.5f;

        Gizmos.color = Color.yellow;
        Gizmos.DrawRay(origin, Vector3.down * groundCheckRayLen);
        Gizmos.DrawRay(origin + transform.right * -halfR, Vector3.down * groundCheckRayLen);
        Gizmos.DrawRay(origin + transform.right * halfR, Vector3.down * groundCheckRayLen);
    }
}