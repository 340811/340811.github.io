using UnityEngine;

/// 挂载在主摄像机上，实现类似 Superliminal 的强制透视抓取
public class PerspectiveGrabber : MonoBehaviour
{
    [SerializeField] private LayerMask grabbableLayer;
    [SerializeField] private LayerMask environmentLayer;
    [SerializeField] private float maxGrabDistance = 50f;
    [SerializeField] private float wallOffset = 0.1f;

    private GameObject heldObject;
    private Rigidbody heldRb;
    private Transform originalParent;

    private float grabDist;          // 抓取瞬间摄像机前方的距离
    private Vector3 grabLocalScale;  // 抓取瞬间的本地缩放

    private void Awake()
    {
        if (grabbableLayer == 0)
            grabbableLayer = LayerMask.GetMask("Grabbable");
        if (environmentLayer == 0)
            environmentLayer = LayerMask.GetMask("Environment");
    }

    void Update()
    {
        if (Input.GetMouseButtonDown(0))
            TryGrab();
        else if (Input.GetMouseButtonUp(0) && heldObject)
            Release();

        if (heldObject)
            UpdateHeld();
    }

    void TryGrab()
    {
        Ray ray = new Ray(transform.position, transform.forward);
        if (!Physics.Raycast(ray, out RaycastHit hit, maxGrabDistance, grabbableLayer))
            return;

        heldObject = hit.collider.gameObject;
        heldRb = heldObject.GetComponent<Rigidbody>();

        if (heldRb)
        {
            heldRb.velocity = Vector3.zero;
            heldRb.angularVelocity = Vector3.zero;
            heldRb.isKinematic = true;
        }

        originalParent = heldObject.transform.parent;
        heldObject.transform.SetParent(transform, true);

        grabDist = heldObject.transform.localPosition.z;
        grabLocalScale = heldObject.transform.localScale;
    }

    void UpdateHeld()
    {
        Ray ray = new Ray(transform.position, transform.forward);
        float targetDist = maxGrabDistance;

        if (Physics.Raycast(ray, out RaycastHit hit, maxGrabDistance, environmentLayer))
        {
            targetDist = Mathf.Max(hit.distance - wallOffset, 0.3f);  // 避免推到摄像机背后
        }

        // 保持屏幕上的视觉大小不变
        float scaleFactor = grabDist > 0.001f ? targetDist / grabDist : 1f;
        heldObject.transform.localScale = grabLocalScale * scaleFactor;

        Vector3 localPos = heldObject.transform.localPosition;
        localPos.z = targetDist;
        heldObject.transform.localPosition = localPos;
    }

    void Release()
    {
        if (heldRb)
            heldRb.isKinematic = false;

        // 原始父节点可能已被销毁，做个保护
        heldObject.transform.SetParent(originalParent ? originalParent : null, true);

        heldObject = null;
        heldRb = null;
    }
}