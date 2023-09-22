using System;
using UnityEngine;

public class MoveCube : MonoBehaviour
{
    private Vector3 _rotationInc;
    private Vector3 _positionInc;
    private bool _shouldMove;
    private double _x;

    private void Start()
    {
        transform.position = MoveCircular(0, 0);
    }

    // Update is called once per frame
    private void Update()
    {
        _shouldMove ^= Input.GetKeyDown("m");
        transform.position = MoveCircular(Time.deltaTime, 3);
    }

    Vector3 MoveCircular(double timing, float scale)
    {
        if (_shouldMove) _x += timing * scale;
        return new Vector3((float)Math.Sin(_x), (float)Math.Cos(_x), 0);
    }
}
